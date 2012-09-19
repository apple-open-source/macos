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


#import "DecodeAudioInterfaceDescriptor.h"

@implementation DecodeAudioInterfaceDescriptor
+(void)decodeBytes:(UInt8 *)descriptor forDevice:(BusProbeDevice *)thisDevice {
	if ( [[thisDevice lastInterfaceClassInfo]protocolNum] < 0x20 )
		decodeBytes10( descriptor, thisDevice );
	else
		decodeBytes20( descriptor, thisDevice );
}

// <rdar://10009579> This method uses the same text as the old DUMPIT() macro,
// but it only prints kUSBAudioDescriptorBytesPerLine bytes per line. 
void dumpIt ( UInt8 *descriptor, BusProbeDevice * thisDevice, int depth )
{
    char			descriptorString[kUSBAudioMaxDescriptorStringSize];
    char			byteAndSpace[4];
    UInt8           offset = 0;
    UInt8           length;
        
    if ( descriptor )
    {
        length = descriptor[0];
        descriptorString[0] = '\0';
        while ( offset < length )
        {
            snprintf ( byteAndSpace, 4, "%02X ", descriptor[offset++] );
            strlcat ( descriptorString, byteAndSpace, kUSBAudioMaxDescriptorStringSize );
            if ( 0 == ( offset % kUSBAudioDescriptorBytesPerLine ) )  // wrap
            {
                [thisDevice addProperty:"Dump Contents (hex):" withValue:descriptorString atDepth:INTERFACE_LEVEL+depth];
                descriptorString[0] = '\0';
            }
            
        }
        // Add the last line
        if ( offset % kUSBAudioDescriptorBytesPerLine )
        {
            [thisDevice addProperty:"Dump Contents (hex):" withValue:descriptorString atDepth:INTERFACE_LEVEL+depth];
        }
    }
}

void decodeBytes10( UInt8 *descriptor, BusProbeDevice * thisDevice ) {
    static  char			buf[256];
    static  char			buf2[256];
    auto AudioCtrlHdrDescriptorPtr		pAudioHdrDesc = NULL;
    auto AudioCtrlInTermDescriptorPtr		pAudioInTermDesc;
    auto AudioCtrlOutTermDescriptorPtr		pAudioOutTermDesc;
    auto AudioCtrlMixerDescriptorPtr		pAudioMixerDesc;
    auto AudioCtrlSelectorDescriptorPtr		pAudioSelectorDesc;
    auto AudioCtrlFeatureDescriptorPtr		pAudioFeatureDesc;
    auto AudioCtrlExtDescriptorPtr		pAudioExtDesc;
    auto acProcessingDescriptorPtr		pAudioProcDesc;
    auto acProcessingDescriptorContPtr		pAudioProcContDesc;
    auto CSAS_InterfaceDescriptorPtr		pAudioGeneralDesc;
    auto CSAS_FormatTypeIDescPtr		pAudioFormatTypeDesc;
    UInt16					i,n,ch, freqIndex, tempIndex;
	UInt16					audioHeaderDescVersion = 0;
    UInt8					*p, *srcIdPtr;
    char					*s;
	bool					addedAttribute;
    UInt16					srcIndex;
    GenericAudioDescriptorPtr			desc = (GenericAudioDescriptorPtr) descriptor;

    if ( ((GenericAudioDescriptorPtr)desc)->descType == CS_ENDPOINT )
    {
        IOUSBEndpointDescriptor	*	pEndpointDesc = ( IOUSBEndpointDescriptor * ) desc;

        sprintf((char *)buf, "Class-Specific AS Audio EndPoint"); 

        [thisDevice addProperty:buf withValue:"" atDepth:(int)INTERFACE_LEVEL];

        sprintf((char *)buf, "0x%02x  %s %s %s", pEndpointDesc->bmAttributes,
                ((pEndpointDesc->bmAttributes & 0x01) == 0x01) ? "Sample Frequency,":"",
                ((pEndpointDesc->bmAttributes & 0x02) == 0x02) ? "Pitch,":"",
                ((pEndpointDesc->bmAttributes & 0x80) == 0x80) ? "MaxPacketsOnly":"" );
        [thisDevice addProperty:"Attributes:" withValue:buf atDepth:INTERFACE_LEVEL+1];

        sprintf((char *)buf, "0x%02x  %s", ((CSAS_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits,
                (0 == ((CSAS_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "(UNDEFINED)" :
                (1 == ((CSAS_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "(Milliseconds)" :
                (2 == ((CSAS_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "(Decoded PCM Samples)" :
                "(RESERVED)" );
        [thisDevice addProperty:"bLockDelayUnits:" withValue:buf atDepth:INTERFACE_LEVEL+1];

        sprintf((char *)buf, "%d %s", Swap16(&((CSAS_IsocEndPtDescPtr)pEndpointDesc)->wLockDelay),
                (1 == ((CSAS_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "ms" :
                (2 == ((CSAS_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "Decoded PCM Samples" :
                "" );
        [thisDevice addProperty:"wLockDelay:" withValue:buf atDepth:INTERFACE_LEVEL+1];
    }

    if ( ((GenericAudioDescriptorPtr)desc)->descType != CS_INTERFACE )
        return;

    if ( AC_CONTROL_SUBCLASS == [[thisDevice lastInterfaceClassInfo] subclassNum] )
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            case ACS_HEADER:
                sprintf((char *)buf, "Audio Control Class Specific Header");
                break;
            case ACS_INPUT_TERMINAL:
                sprintf((char *)buf, "Audio Class Specific Input Terminal");
                break;
            case ACS_OUTPUT_TERMINAL:
                sprintf((char *)buf, "Audio Class Specific Output Terminal");
                break;
            case ACS_MIXER_UNIT:
                sprintf((char *)buf, "Audio Class Specific Mixer Unit");
                break;
            case ACS_SELECTOR_UNIT:
                sprintf((char *)buf, "Audio Class Specific Selector Unit");
                break;
            case ACS_FEATURE_UNIT:
                sprintf((char *)buf, "Audio Class Specific Feature");
                break;
            case ACS_PROCESSING_UNIT:
                sprintf((char *)buf, "Audio Class Specific Processing Unit");
                break;
            case ACS_EXTENSION_UNIT:
                sprintf((char *)buf, "Audio Class Specific Extension");
                break;
	    default:
                sprintf((char *)buf, "Uknown AC_CONTROL_SUBCLASS SubClass");
        }
    }
    else if ( AC_STREAM_SUBCLASS == [[thisDevice lastInterfaceClassInfo] subclassNum] )
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            case ACS_HEADER:
                sprintf((char *)buf, "Audio Control Class Specific Header");
                break;
            case ACS_FORMAT_TYPE:
                sprintf((char *)buf, "Audio Class Specific Audio Data Format");
                break;
                break;
	    default:
                sprintf((char *)buf, "Uknown AC_STREAM_SUBCLASS Type");
        }
    }
    else
	sprintf((char *)buf, "Uknown Interface SubClass Type");
    
    [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL];


    if ( [[thisDevice lastInterfaceClassInfo] subclassNum]==0x01  ) // Audio Control
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            // Once the header is read, should the code read the rest of the desc. since the
            //    happens to include a total size?
            // Refer to USB Device Class Definition for Audio Devices 1.0 p. 37.
            case ACS_HEADER:
                pAudioHdrDesc = (AudioCtrlHdrDescriptorPtr)desc;
                audioHeaderDescVersion = Swap16(&pAudioHdrDesc->descVersNum);
                sprintf((char *)buf, "%1x%1x.%1x%1x",
                        (audioHeaderDescVersion>>12)&0x000f, (audioHeaderDescVersion>>8)&0x000f, (audioHeaderDescVersion>>4)&0x000f, (audioHeaderDescVersion>>0)&0x000f );
                [thisDevice addProperty:"Descriptor Version Number:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", Swap16(&pAudioHdrDesc->descTotalLength) );
                [thisDevice addProperty:"Class Specific Size:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioHdrDesc->descAICNum );
                [thisDevice addProperty:"Number of Audio Interfaces:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                //Haven't seen this array filled with more than 1 yet.
                for (i=0,p=&pAudioHdrDesc->descInterfaceNum[0]; i<pAudioHdrDesc->descAICNum; i++,p++ )
                {
                    sprintf((char *)buf, "%u", *p );
                    [thisDevice addProperty:"Audio Interface Number:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                dumpIt ( ( UInt8 * )pAudioHdrDesc, thisDevice, 1 );
                    break;


            case ACS_INPUT_TERMINAL:
                pAudioInTermDesc = (AudioCtrlInTermDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_INPUT_TERMINAL" );
                //// AddStringChild(item, buf);
                sprintf((char *)buf, "%u", pAudioInTermDesc->descTermID );
                [thisDevice addProperty:"Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // To get input terminal types refer to USB PDF files for Termt10.pdf pp. 7-8.
                switch ( Swap16(&pAudioInTermDesc->descTermType) )
                {
                    //Should 0x0100 to 0x01ff be included here?  They are not input terminal
                    //   types, but Jazz says they are.
                    case 0x100: s="USB Undefined"; break;
                    case 0x101: s="USB streaming"; break;
                    case 0x1ff: s="USB vendor specific"; break;

                    case 0x200: s="Input Undefined"; break;
                    case 0x201: s="Microphone"; break;
                    case 0x202: s="Desktop microphone"; break;
                    case 0x203: s="Personal microphone"; break;
                    case 0x204: s="Omni-directional microphone"; break;
                    case 0x205: s="Microphone array"; break;
                    case 0x206: s="Processing microphone array"; break;

                    case 0x400: s="Bi-directional Terminal, Undefined"; break;
                    case 0x401: s="Bi-directional Handset"; break;
                    case 0x402: s="Bi-directional Headset"; break;
                    case 0x403: s="Bi-directional Speakerphone (no echo reduction)"; break;
                    case 0x404: s="Bi-directional Speakerphone (echo supression)"; break;
                    case 0x405: s="Bi-directional Speakerphone (echo canceling)"; break;

                    case 0x500: s="Telephony Terminal, Undefined"; break;
                    case 0x501: s="Telephony Phoneline"; break;
                    case 0x502: s="Telephony Telephone"; break;
                    case 0x503: s="Telephony Down Line Phone"; break;

                    case 0x600: s="External Undefined"; break;
                    case 0x601: s="Analog connector"; break;
                    case 0x602: s="Digital audio connector"; break;
                    case 0x603: s="Line connector"; break;
                    case 0x604: s="Legacy audio connector"; break;
                    case 0x605: s="S/PDIF interface"; break;
                    case 0x606: s="1394 DA stream"; break;
                    case 0x607: s="1394 DV stream soundtrack"; break;

                    case 0x700: s="Embedded Terminal Undefined"; break;
                    case 0x703: s="Embedded Audio CD"; break;
                    case 0x704: s="Embedded Digital Audio Tape"; break;
                    case 0x705: s="Embedded Digital Compact Cassette"; break;
                    case 0x706: s="Embedded MiniDisk"; break;
                    case 0x707: s="Embedded Analog Tape"; break;
                    case 0x708: s="Embedded Vinyl Record Player"; break;
                    case 0x709: s="Embedded VCR Audio"; break;
                    case 0x70a: s="Embedded Video Disk Audio"; break;
                    case 0x70b: s="Embedded DVD Audio"; break;
                    case 0x70c: s="Embedded TV Tuner Audio"; break;
                    case 0x70d: s="Embedded Satellite Receiver Audio"; break;
                    case 0x70e: s="Embedded Cable Tuner Audio"; break;
                    case 0x70f: s="Embedded DSS Audio"; break;
                    case 0x710: s="Embedded Radio Receiver"; break;
                    case 0x712: s="Embedded Multi-track Recorder"; break;
                    case 0x713: s="Embedded Synthesizer"; break;
                    default: s="Invalid Input Terminal Type";
                }

                sprintf((char *)buf, 	"0x%x (%s)", pAudioInTermDesc->descTermType, s );
                [thisDevice addProperty:"Input Terminal Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                if ( !pAudioInTermDesc->descOutTermID )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioInTermDesc->descOutTermID );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioInTermDesc->descOutTermID );
                    }
                    [thisDevice addProperty:"OutTerminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioInTermDesc->descNumChannels );
                [thisDevice addProperty:"Number of Channels:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                Swap16(&pAudioInTermDesc->descChannelConfig);
                sprintf((char *)buf, "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 15 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 14 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 13 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 12 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 11 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 10 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  9 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  8 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  7 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  6 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  5 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  4 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  3 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  2 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  1 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  0 ) ? 1 : 0 );
                [thisDevice addProperty:"Spatial config of channels:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                for( tempIndex = 0; tempIndex < 12; tempIndex++ )
                {
                    if ( pAudioInTermDesc->descChannelConfig & (1 <<  tempIndex) )
                    {
                        switch( tempIndex )
                        {
                            case 11:	sprintf((char *)buf, "    ^ииииииииииии  Top");			break;
                            case 10:	sprintf((char *)buf, "     ^иииииииииии  Side Right");		break;
                            case 9:		sprintf((char *)buf, "      ^ииииииииии  Side Left");		break;
                            case 8:		sprintf((char *)buf, "       ^иииииииии  Surround");		break;
                            case 7:		sprintf((char *)buf, "        ^ииииииии  Right of Center");	break;
                            case 6:		sprintf((char *)buf, "         ^иииииии  Left of Center");	break;
                            case 5:		sprintf((char *)buf, "          ^ииииии  Right Surround");	break;
                            case 4:		sprintf((char *)buf, "           ^иииии  Left Surround");	break;
                            case 3:		sprintf((char *)buf, "            ^ииии  Low Frequency Effects");	break;
                            case 2:		sprintf((char *)buf, "             ^иии  Center");		break;
                            case 1:		sprintf((char *)buf, "              ^ии  Right Front");		break;
                            case 0:		sprintf((char *)buf, "               ^и  Left Front");		break;
                        }
                        [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+1];
                    }
                }

                sprintf((char *)buf, "%u", pAudioInTermDesc->descChannelNames );
                [thisDevice addProperty:"String index for first logical channel:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                if ( !pAudioInTermDesc->descTermName )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioInTermDesc->descTermName );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioInTermDesc->descTermName );
                    }
                    [thisDevice addProperty:"Terminal Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                break;

            case ACS_OUTPUT_TERMINAL:
                pAudioOutTermDesc = (AudioCtrlOutTermDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_OUTPUT_TERMINAL" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioOutTermDesc->descTermID );
                [thisDevice addProperty:"Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // To get output terminal types refer to USB PDF files for Termt10.pdf p. 8.
                switch ( Swap16(&pAudioOutTermDesc->descTermType) )
                {
                    case 0x100: s="USB Undefined"; break;
                    case 0x101: s="USB Isochronous Stream"; break;
                    case 0x1ff: s="USB vendor specific"; break;

                    case 0x0300: s="Output Undefined"; break;
                    case 0x0301: s="Speaker"; break;
                    case 0x0302: s="Headphones"; break;
                    case 0x0303: s="Head Mounted Display Audio"; break;
                    case 0x0304: s="Desktop speaker"; break;
                    case 0x0305: s="Room speaker"; break;
                    case 0x0306: s="Communication speaker"; break;
                    case 0x0307: s="Low frequency effects speaker"; break;

                    case 0x400: s="Bi-directional Terminal, Undefined"; break;
                    case 0x401: s="Bi-directional Handset"; break;
                    case 0x402: s="Bi-directional Headset"; break;
                    case 0x403: s="Bi-directional Speakerphone (no echo reduction)"; break;
                    case 0x404: s="Bi-directional Speakerphone (echo supression)"; break;
                    case 0x405: s="Bi-directional Speakerphone (echo canceling)"; break;

                    case 0x500: s="Telephony Terminal, Undefined"; break;
                    case 0x501: s="Telephony Phoneline"; break;
                    case 0x502: s="Telephony Telephone"; break;
                    case 0x503: s="Telephony Down Line Phone"; break;

                    case 0x600: s="External Undefined"; break;
                    case 0x601: s="Analog connector"; break;
                    case 0x602: s="Digital audio connector"; break;
                    case 0x603: s="Line connector"; break;
                    case 0x604: s="Legacy audio connector"; break;
                    case 0x605: s="S/PDIF interface"; break;
                    case 0x606: s="1394 DA stream"; break;
                    case 0x607: s="1394 DV stream soundtrack"; break;

                    case 0x700: s="Embedded Terminal Undefined"; break;
                    case 0x701: s="Embedded Level Calibration Noise"; break;
                    case 0x702: s="Embedded Equalization Noise"; break;
                    case 0x704: s="Embedded Digital Audio Tape"; break;
                    case 0x705: s="Embedded Digital Compact Cassette"; break;
                    case 0x706: s="Embedded MiniDisk"; break;
                    case 0x707: s="Embedded Analog Tape"; break;
                    case 0x708: s="Phonograph"; break;
                    case 0x709: s="VCR Audio"; break;
                    case 0x70A: s="Video Disc Audio"; break;
                    case 0x70B: s="DVD Audio"; break;
                    case 0x70C: s="TV Tuner Audio"; break;
                    case 0x70D: s="Satellite Receiver Audio"; break;
                    case 0x70E: s="Cable Tuner Audio"; break;
                    case 0x70F: s="DSS Audio"; break;
                    case 0x710: s="Radio Receiver"; break;
                    case 0x711: s="Embedded AM/FM Radio Transmitter"; break;
                    case 0x712: s="Embedded Multi-track Recorder"; break;
                    case 0x713: s="Synthesizer"; break;
                    default: s="Invalid Output Terminal Type";
                }

                sprintf((char *)buf, 	"0x%x (%s)", pAudioOutTermDesc->descTermType, s );
                [thisDevice addProperty:"Output Terminal Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                if ( !pAudioOutTermDesc->descInTermID )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioOutTermDesc->descInTermID );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioOutTermDesc->descInTermID );
                    }
                    [thisDevice addProperty:"InTerminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];


                sprintf((char *)buf, "%u", pAudioOutTermDesc->descSourceID );
                [thisDevice addProperty:"Source ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];


                if ( !pAudioOutTermDesc->descTermName )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioOutTermDesc->descTermName );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioOutTermDesc->descTermName );
                    }
                    [thisDevice addProperty:"Terminal Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                break;
            case ACS_MIXER_UNIT:
                pAudioMixerDesc = (AudioCtrlMixerDescriptorPtr)desc;
                sprintf((char *)buf, "ACS_MIXER_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioMixerDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pAudioMixerDesc->descNumPins );
                [thisDevice addProperty:"Number of pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // Add parsing for other pin IDs and channel configurations.
                for( i=0; i < pAudioMixerDesc->descNumPins; i++ )
                {

                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i+1);
                    sprintf((char *)buf,	"%u", pAudioMixerDesc->descSourcePID[i] );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                dumpIt ( ( UInt8 * )pAudioMixerDesc, thisDevice, 1 );
                    break;

            case ACS_SELECTOR_UNIT:
                pAudioSelectorDesc = (AudioCtrlSelectorDescriptorPtr)desc;
                sprintf((char *)buf, "ACS_SELECTOR_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioSelectorDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pAudioSelectorDesc->descNumPins );
                [thisDevice addProperty:"Number of pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // Add parsing for other pin IDs and channel configurations.
                for( i=0; i < pAudioSelectorDesc->descNumPins; i++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf,	"%u", pAudioSelectorDesc->descSourcePID[i] );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                dumpIt ( ( UInt8 * )pAudioSelectorDesc, thisDevice, 1 );
                    break;

            case ACS_FEATURE_UNIT:
                pAudioFeatureDesc = (AudioCtrlFeatureDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_FEATURE_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioFeatureDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pAudioFeatureDesc->descSourceID );
                [thisDevice addProperty:"Source ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pAudioFeatureDesc->descCtrlSize );
                [thisDevice addProperty:"Control Size:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // The feature descriptor length equals len=7+(ch+1)*n from Audio Class
                //    Devices p. 43).
                //
                // Algebra:
                //        len - 7 = (ch + 1)*n
                //        (len-7)/n = ch + 1
                //        ((len-7)/n) - 1 = ch
                //
                // So, ch = ((len-7)/n) - 1;
                //
				if ( audioHeaderDescVersion < 0x0200 )
				{
					n = pAudioFeatureDesc->descCtrlSize;
					ch = ((pAudioFeatureDesc->descLen - 7)/n) - 1;
				} else {
					n = 4;
					ch = ((pAudioFeatureDesc->descLen - 6)/n) - 1;
				}
					sprintf((char *)buf,	"%u", ch );
                [thisDevice addProperty:"Number of Channels (ch):" withValue:buf atDepth:INTERFACE_LEVEL+1];

                p=&pAudioFeatureDesc->descControls[0];
                for ( tempIndex = 0; tempIndex <= ch; tempIndex++ )
                {
                    buf[0] = 0;
					s = (char*)buf;
                    switch( tempIndex )
                    {
                        case 0:		strcat (s, "Master Channel:......................... ( " );	break;
                        case 1:		strcat (s, "Left Front:............................. ( " );	break;
                        case 2:		strcat (s, "Right Front:............................ ( " );	break;
                        case 3:		strcat (s, "Center Front:........................... ( " );	break;
                        case 4:		strcat (s, "Low Frequency Enhancement:.............. ( " );	break;
                        case 5:		strcat (s, "Left Surround:.......................... ( " );	break;
                        case 6:		strcat (s, "Right Surround:......................... ( " );	break;
                        case 7:		strcat (s, "Left of Center:......................... ( " );	break;
                        case 8:		strcat (s, "Right of Center:........................ ( " );	break;
                        case 9:		strcat (s, "Surround:............................... ( " );	break;
                        case 10:	strcat (s, "Side Left:.............................. ( " );	break;
                        case 11:	strcat (s, "Side Right:............................. ( " );	break;
                        case 12:	strcat (s, "Side Left:.............................. ( " );	break;
                        default:	strcat (s, "Unknown spatial relation:............... ( " );	break;
                    }

                    // The number of bytes for each field is indicated by descCtrlSize.
                    n = pAudioFeatureDesc->descCtrlSize;
					addedAttribute = FALSE;
                    switch( n )
                    {
                        case 2:		//	10 attribute bits supported by Audio Class 1.0 specificationи
                            if ( p[1] & (~((1<<0)|(1<<1))) ) strcat (s,	"Reserved, " ); 	// D15-D10
                            if ( p[1] & (1<<1) ) strcat (s,	"Loudness, " );		// D9
                            if ( p[1] & (1<<0) ) strcat (s, "Bass Boost, " );	// D8
                                        //	FALL THROUGH!!!!!
                        case 1:		//	10 attribute bits supported by Audio Class 1.0 specificationи
                            if ( ( 0 != p[1] || 0 != p[0] ) ) addedAttribute = TRUE;
							if ( p[0] & (1<<7) ) strcat (s, "Delay, " );		// D7
                            if ( p[0] & (1<<6) ) strcat (s, "Automatic Gain, " );	// D6
                            if ( p[0] & (1<<5) ) strcat (s, "Graphic Equalizer, " );// D5
                            if ( p[0] & (1<<4) ) strcat (s, "Treble, " ); 		// D4
                            if ( p[0] & (1<<3) ) strcat (s, "Midi, " ); 		// D3
                            if ( p[0] & (1<<2) ) strcat (s, "Bass, " ); 		// D2
                            if ( p[0] & (1<<1) ) strcat (s, "Volume, " );  		// D1
                            if ( p[0] & (1<<0) ) strcat (s, "Mute, " );		// D0
                            break;
                        default:
                            strcat (s, "╔╔ UNDEFINED ATTRIBUTES ╔╔  " );
                            break;
                    }
                    p += n;
					
					// Destroy trailing comma or spaces by terminating string early if necessary
                    if (addedAttribute)
					{
						*(s + strlen(s) - 2) = 0;
                    }
					strcat (s, " )" );
					[thisDevice addProperty:" " withValue:s atDepth:INTERFACE_LEVEL+1];
                    // AddStringChild(item, buf);
                }

                    // p points to the next descriptor byte.
                    sprintf((char *)buf, "%u", *p );
                [thisDevice addProperty:"Feature Unit Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				// Dump hex values for Audio Class Specific Feature
                dumpIt ( ( UInt8 * )pAudioFeatureDesc, thisDevice, 1 );
                break;
            case ACS_EXTENSION_UNIT:
                pAudioExtDesc = (AudioCtrlExtDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_EXTENSION_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, "%u", pAudioExtDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%x", Swap16(&pAudioExtDesc->descExtensionCode) );
                [thisDevice addProperty:"Vendor Ext. Code:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioExtDesc->descNumPins );
                [thisDevice addProperty:"Number of Input Pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // The feature descriptor length equals len=13+descNumPins+bControlSize
                //    from Audio Class Devices p. 56.

                // Add parsing for other pin IDs and channel configurations.
                for( i=0; i < pAudioExtDesc->descNumPins; i++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf,	"%u", pAudioExtDesc->descSourcePID[i] );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                dumpIt ( ( UInt8 * )pAudioExtDesc, thisDevice, 1 );
                break;
            case ACS_PROCESSING_UNIT:
                pAudioProcDesc = (acProcessingDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_PROCESSING_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, "%u", pAudioProcDesc->bUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%04x", Swap16(&pAudioProcDesc->wProcessType) );
                [thisDevice addProperty:"Process Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioProcDesc->bNrPins );
                [thisDevice addProperty:"Number of Input Pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                for( srcIndex = 0, srcIdPtr = (UInt8*)&pAudioProcDesc->bNrPins; srcIndex < pAudioProcDesc->bNrPins; srcIndex++ )
                {
                    sprintf((char *)buf2, "SourceID(%u):", srcIndex);
                    sprintf((char *)buf, "%u", *srcIdPtr++ );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                    pAudioProcContDesc = (acProcessingDescriptorContPtr)srcIdPtr;
                sprintf((char *)buf, "%u", pAudioProcContDesc->bNrChannels );
                [thisDevice addProperty:"Number of Channels:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%04x", Swap16(&pAudioProcContDesc->wChannelConfig ));
                [thisDevice addProperty:"Channel Configuration:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioProcContDesc->iChannelNames );
                [thisDevice addProperty:"Channel Names:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioProcContDesc->bControlSize );
                [thisDevice addProperty:"Control Size:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%04x", Swap16(&pAudioProcContDesc->bmControls ));
                [thisDevice addProperty:"Controls:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioProcContDesc->iProcessing );
                [thisDevice addProperty:"Process Unit Name:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                dumpIt ( ( UInt8 * )pAudioProcDesc, thisDevice, 1 );
                break;
	}
            else            if ( [[thisDevice lastInterfaceClassInfo] subclassNum]==0x02 /*AudioStreaming*/ )
                switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
                {

                    case ACS_ASTREAM_GENERAL:
                        sprintf((char *)buf, "Audio Stream General" );
                        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL+1];

                        pAudioGeneralDesc = (CSAS_InterfaceDescriptorPtr)desc;
                        sprintf((char *)buf, "%u", pAudioGeneralDesc->terminalID );
                        [thisDevice addProperty:"Endpoint Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u frames     %s", pAudioGeneralDesc->delay,
                                ( 0 == pAudioGeneralDesc->delay) ? "(Delay NOT SUPPORTED)":"");
                        [thisDevice addProperty:"Delay:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        i = Swap16(&pAudioGeneralDesc->formatTag);

                        switch (i)
                        {
                            case 0x0000:
                                s = "TYPE_I_UNDEFINED";
                                break;
                            case 0x0001:
                                //The PCM (Pulse Coded Modulation) format is the most commonly used
                                //   audio format to represent audio data streams. The audio data is
                                //   not compressed and uses a signed twoуs-complement fixed point
                                //   format. It is left-justified (the sign bit is the Msb) and data
                                //   is padded with trailing zeros to fill the remaining unused bits
                                //   of the subframe. The binary point is located to the right of the
                                //   sign bit so that all values lie within the range [-1,+1).
                                s = "PCM";
                                break;
                            case 0x0002:
                                s = "PCM8";
                                break;
                            case 0x0003:
                                s = "IEEE_FLOAT";
                                break;
                            case 0x0004:
                                s = "ALAW";
                                break;
                            case 0x0005:
                                s = "MULAW";
                                break;
                            default:
                                s = "Unknown Format ID";				
			}
                            sprintf((char *)buf, "0x%.4x (%s)", i, s );
                        [thisDevice addProperty:"Format Tag:" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        break;
                    case ACS_ASTREAM_TYPE:
                        sprintf((char *)buf, "Audio Stream Format Type Desc." );
                        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL+1];

                        pAudioFormatTypeDesc = (CSAS_FormatTypeIDescPtr)desc;
                        sprintf((char *)buf, "%u %s", pAudioFormatTypeDesc->formatType, (1==pAudioFormatTypeDesc->formatType ? "PCM" : "NOT Supported by MacOS") );
                        [thisDevice addProperty:"Format Type:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u %s", pAudioFormatTypeDesc->numberOfChannels, (2==pAudioFormatTypeDesc->numberOfChannels ? "STEREO" : (1==pAudioFormatTypeDesc->numberOfChannels ? "MONO" : "MULTICHANNEL")) );
                        [thisDevice addProperty:"Number Of Channels:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u", pAudioFormatTypeDesc->subFrameSize );
                        [thisDevice addProperty:"Sub Frame Size:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u", pAudioFormatTypeDesc->bitResolution );
                        [thisDevice addProperty:"Bit Resolution:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "0x%.2x (%s)", pAudioFormatTypeDesc->sampleFreqType,
                                (pAudioFormatTypeDesc->sampleFreqType ? "Discrete" : "Continuous") );
                        [thisDevice addProperty:"Sample Frequency Type:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        if (!pAudioFormatTypeDesc->sampleFreqType )
                        {
                            CSAS_Freq3Ptr  b3;
                            UInt32 u;
                            b3 = &pAudioFormatTypeDesc->sampleFreqTables.cont.lowerSamFreq;	// this is byte swapped!!!
                            u = (b3->byte1 << 0) | (b3->byte2 << 8) | (b3->byte3 << 16);
                            sprintf((char *)buf, "%5u Hz", (int)u );
                            [thisDevice addProperty:"Lower Sample Freq:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                            b3 = &pAudioFormatTypeDesc->sampleFreqTables.cont.upperSamFreq;
                            u = (b3->byte1 << 0) | (b3->byte2 << 8) | (b3->byte3 << 16);
                            sprintf((char *)buf, "%5u Hz", (int)u );
                            [thisDevice addProperty:"Upper Sample Freq:" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        }
                            else
                            {
                                CSAS_Freq3Ptr		b4;
                                UInt32				u;
                                b4 = (CSAS_Freq3Ptr)&pAudioFormatTypeDesc->sampleFreqTables.discrete;
                                for( freqIndex = 0; freqIndex < pAudioFormatTypeDesc->sampleFreqType; freqIndex++ )
                                {
                                    u = (b4[freqIndex].byte1 << 0) | (b4[freqIndex].byte2 << 8) | (b4[freqIndex].byte3 << 16);
                                    sprintf((char *)buf, "%5u Hz", (int)u );
                                    [thisDevice addProperty:"Sample Frequency:" withValue:buf atDepth:INTERFACE_LEVEL+2];
                                }
                            }
                            break;
                    default:
                        sprintf((char *)buf, "AudioStreaming Subclass" );
                        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL+1];
	}
                    else if ( [[thisDevice lastInterfaceClassInfo] subclassNum]==0x03 /*MIDIStreaming*/ )
                        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
                        {
                        }
}

void decodeBytes20( UInt8 *descriptor, BusProbeDevice * thisDevice ) {
    static  char			buf[256];
    static  char			buf2[256];
    auto Audio20CtrlHdrDescriptorPtr		pAudioHdrDesc;
    auto Audio20CtrlInTermDescriptorPtr		pAudioInTermDesc;
    auto Audio20ClockSourceDescriptorPtr		pAudioClockSourceDesc;
	auto Audio20ClockSelectorDescriptorPtr		pAudioClockSelectorDesc;
	auto Audio20ClockMultiplierDescriptorPtr	pAudioClockMultiplierDesc;
    auto Audio20CtrlOutTermDescriptorPtr		pAudioOutTermDesc;
    auto AudioCtrlMixerDescriptorPtr		pAudioMixerDesc;
    auto AudioCtrlSelectorDescriptorPtr		pAudioSelectorDesc;
    auto Audio20CtrlFeatureDescriptorPtr		pAudioFeatureDesc;
    auto Audio20CtrlExtDescriptorPtr		pAudioExtDesc;
    auto ac20ProcessingDescriptorPtr		pAudioProcDesc;
    auto ac20ProcessingDescriptorContPtr		pAudioProcContDesc;
    auto CS20AS_InterfaceDescriptorPtr		pAudioGeneralDesc;
    auto CS20AS_FormatTypeIDescPtr		pAudioFormatTypeDesc;
    UInt16					i,n,ch,tempIndex;
    UInt8					*p, *srcIdPtr, *bufPtr;
    char					*s;
	bool					addedAttribute;
    UInt16					srcIndex;
    GenericAudioDescriptorPtr			desc = (GenericAudioDescriptorPtr) descriptor;

    if ( ((GenericAudioDescriptorPtr)desc)->descType == CS_ENDPOINT )
    {
        IOUSBEndpointDescriptor	*	pEndpointDesc = ( IOUSBEndpointDescriptor * ) desc;

        sprintf((char *)buf, "Class-Specific AS Audio EndPoint"); 

        [thisDevice addProperty:buf withValue:"" atDepth:(int)INTERFACE_LEVEL];

        sprintf((char *)buf, "0x%02x  %s", pEndpointDesc->bmAttributes,
                ((pEndpointDesc->bmAttributes & 0x80) == 0x80) ? "MaxPacketsOnly":"" );
        [thisDevice addProperty:"Attributes:" withValue:buf atDepth:INTERFACE_LEVEL+1];
		
		sprintf((char *)buf, "0x%02x", ((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->bmControls);
        [thisDevice addProperty:"bmControls:" withValue:buf atDepth:INTERFACE_LEVEL+1];

        sprintf((char *)buf, "0x%02x  %s", ((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits,
                (0 == ((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "(UNDEFINED)" :
                (1 == ((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "(Milliseconds)" :
                (2 == ((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "(Decoded PCM Samples)" :
                "(RESERVED)" );
        [thisDevice addProperty:"bLockDelayUnits:" withValue:buf atDepth:INTERFACE_LEVEL+1];

        sprintf((char *)buf, "%d %s", Swap16(&((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->wLockDelay),
                (1 == ((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "ms" :
                (2 == ((CSAS20_IsocEndPtDescPtr)pEndpointDesc)->bLockDelayUnits) ? "Decoded PCM Samples" :
                "" );
        [thisDevice addProperty:"wLockDelay:" withValue:buf atDepth:INTERFACE_LEVEL+1];
    }

    if ( ((GenericAudioDescriptorPtr)desc)->descType != CS_INTERFACE )
        return;

    if ( AC_CONTROL_SUBCLASS == [[thisDevice lastInterfaceClassInfo] subclassNum] )
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            case ACS_HEADER:
                sprintf((char *)buf, "Audio Control Class Specific Header");
                break;
            case ACS_INPUT_TERMINAL:
                sprintf((char *)buf, "Audio Class Specific Input Terminal");
                break;
            case AC20S_CLOCK_SOURCE:
                sprintf((char *)buf, "Audio Class Specific Clock Source");
                break;
			case AC20S_CLOCK_SELECTOR:
                sprintf((char *)buf, "Audio Class Specific Clock Selector");
                break;
			case AC20S_CLOCK_MULTIPLIER:
                sprintf((char *)buf, "Audio Class Specific Clock Multiplier");
                break;
            case ACS_OUTPUT_TERMINAL:
                sprintf((char *)buf, "Audio Class Specific Output Terminal");
                break;
            case ACS_MIXER_UNIT:
                sprintf((char *)buf, "Audio Class Specific Mixer Unit");
                break;
            case ACS_SELECTOR_UNIT:
                sprintf((char *)buf, "Audio Class Specific Selector Unit");
                break;
            case ACS_FEATURE_UNIT:
                sprintf((char *)buf, "Audio Class Specific Feature");
                break;
            case ACS_PROCESSING_UNIT:
                sprintf((char *)buf, "Audio Class Specific Processing Unit");
                break;
            case ACS_EXTENSION_UNIT:
                sprintf((char *)buf, "Audio Class Specific Extension");
                break;
	    default:
                sprintf((char *)buf, "Uknown AC_CONTROL_SUBCLASS SubClass");
        }
    }
    else if ( AC_STREAM_SUBCLASS == [[thisDevice lastInterfaceClassInfo] subclassNum] )
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            case ACS_HEADER:
                sprintf((char *)buf, "Audio Control Class Specific Header");
                break;
            case ACS_FORMAT_TYPE:
                sprintf((char *)buf, "Audio Class Specific Audio Data Format");
                break;
                break;
	    default:
                sprintf((char *)buf, "Uknown AC_STREAM_SUBCLASS Type");
        }
    }
    else
	sprintf((char *)buf, "Uknown Interface SubClass Type");
    
    [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL];


    if ( [[thisDevice lastInterfaceClassInfo] subclassNum]==0x01  ) {
		// Audio Control
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            // Once the header is read, should the code read the rest of the desc. since the
            //    happens to include a total size?
            // Refer to USB Device Class Definition for Audio Devices 1.0 p. 37.
            case ACS_HEADER:
                pAudioHdrDesc = (Audio20CtrlHdrDescriptorPtr)desc;
                i = Swap16(&pAudioHdrDesc->descVersNum);
                sprintf((char *)buf, "%1x%1x.%1x%1x",
                        (i>>12)&0x000f, (i>>8)&0x000f, (i>>4)&0x000f, (i>>0)&0x000f );
                [thisDevice addProperty:"Descriptor Version Number:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioHdrDesc->descCategory );
                [thisDevice addProperty:"Category:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", Swap16(&pAudioHdrDesc->descTotalLength) );
                [thisDevice addProperty:"Class Specific Size:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioHdrDesc->descbmControls );
                [thisDevice addProperty:"bmControls:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                dumpIt ( ( UInt8 * )pAudioHdrDesc, thisDevice, 1 );
				break;

            case ACS_INPUT_TERMINAL:
                pAudioInTermDesc = (Audio20CtrlInTermDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_INPUT_TERMINAL" );
                //// AddStringChild(item, buf);
                sprintf((char *)buf, "%u", pAudioInTermDesc->descTermID );
                [thisDevice addProperty:"Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // To get input terminal types refer to USB PDF files for Termt10.pdf pp. 7-8.
                switch ( Swap16(&pAudioInTermDesc->descTermType) )
                {
                    //Should 0x0100 to 0x01ff be included here?  They are not input terminal
                    //   types, but Jazz says they are.
                    case 0x100: s="USB Undefined"; break;
                    case 0x101: s="USB streaming"; break;
                    case 0x1ff: s="USB vendor specific"; break;

                    case 0x200: s="Input Undefined"; break;
                    case 0x201: s="Microphone"; break;
                    case 0x202: s="Desktop microphone"; break;
                    case 0x203: s="Personal microphone"; break;
                    case 0x204: s="Omni-directional microphone"; break;
                    case 0x205: s="Microphone array"; break;
                    case 0x206: s="Processing microphone array"; break;

                    case 0x400: s="Bi-directional Terminal, Undefined"; break;
                    case 0x401: s="Bi-directional Handset"; break;
                    case 0x402: s="Bi-directional Headset"; break;
                    case 0x403: s="Bi-directional Speakerphone (no echo reduction)"; break;
                    case 0x404: s="Bi-directional Speakerphone (echo supression)"; break;
                    case 0x405: s="Bi-directional Speakerphone (echo canceling)"; break;

                    case 0x500: s="Telephony Terminal, Undefined"; break;
                    case 0x501: s="Telephony Phoneline"; break;
                    case 0x502: s="Telephony Telephone"; break;
                    case 0x503: s="Telephony Down Line Phone"; break;

                    case 0x600: s="External Undefined"; break;
                    case 0x601: s="Analog connector"; break;
                    case 0x602: s="Digital audio connector"; break;
                    case 0x603: s="Line connector"; break;
                    case 0x604: s="Legacy audio connector"; break;
                    case 0x605: s="S/PDIF interface"; break;
                    case 0x606: s="1394 DA stream"; break;
                    case 0x607: s="1394 DV stream soundtrack"; break;

                    case 0x700: s="Embedded Terminal Undefined"; break;
                    case 0x703: s="Embedded Audio CD"; break;
                    case 0x704: s="Embedded Digital Audio Tape"; break;
                    case 0x705: s="Embedded Digital Compact Cassette"; break;
                    case 0x706: s="Embedded MiniDisk"; break;
                    case 0x707: s="Embedded Analog Tape"; break;
                    case 0x708: s="Embedded Vinyl Record Player"; break;
                    case 0x709: s="Embedded VCR Audio"; break;
                    case 0x70a: s="Embedded Video Disk Audio"; break;
                    case 0x70b: s="Embedded DVD Audio"; break;
                    case 0x70c: s="Embedded TV Tuner Audio"; break;
                    case 0x70d: s="Embedded Satellite Receiver Audio"; break;
                    case 0x70e: s="Embedded Cable Tuner Audio"; break;
                    case 0x70f: s="Embedded DSS Audio"; break;
                    case 0x710: s="Embedded Radio Receiver"; break;
                    case 0x712: s="Embedded Multi-track Recorder"; break;
                    case 0x713: s="Embedded Synthesizer"; break;
                    default: s="Invalid Input Terminal Type";
                }

                sprintf((char *)buf, 	"0x%x (%s)", pAudioInTermDesc->descTermType, s );
                [thisDevice addProperty:"Input Terminal Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                if ( !pAudioInTermDesc->descOutTermID )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioInTermDesc->descOutTermID );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioInTermDesc->descOutTermID );
                    }
                    [thisDevice addProperty:"OutTerminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioInTermDesc->descNumChannels );
                [thisDevice addProperty:"Number of Channels:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                Swap16(&pAudioInTermDesc->descChannelConfig);
                sprintf((char *)buf, "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 15 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 14 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 13 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 12 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 11 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 << 10 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  9 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  8 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  7 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  6 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  5 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  4 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  3 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  2 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  1 ) ? 1 : 0,
                        (pAudioInTermDesc->descChannelConfig) & ( 1 <<  0 ) ? 1 : 0 );
                [thisDevice addProperty:"Spatial config of channels:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                for( tempIndex = 0; tempIndex < 12; tempIndex++ )
                {
                    if ( pAudioInTermDesc->descChannelConfig & (1 <<  tempIndex) )
                    {
                        switch( tempIndex )
                        {
                            case 11:	sprintf((char *)buf, "    ^ииииииииииии  Top");			break;
                            case 10:	sprintf((char *)buf, "     ^иииииииииии  Side Right");		break;
                            case 9:		sprintf((char *)buf, "      ^ииииииииии  Side Left");		break;
                            case 8:		sprintf((char *)buf, "       ^иииииииии  Surround");		break;
                            case 7:		sprintf((char *)buf, "        ^ииииииии  Right of Center");	break;
                            case 6:		sprintf((char *)buf, "         ^иииииии  Left of Center");	break;
                            case 5:		sprintf((char *)buf, "          ^ииииии  Right Surround");	break;
                            case 4:		sprintf((char *)buf, "           ^иииии  Left Surround");	break;
                            case 3:		sprintf((char *)buf, "            ^ииии  Low Frequency Effects");	break;
                            case 2:		sprintf((char *)buf, "             ^иии  Center");		break;
                            case 1:		sprintf((char *)buf, "              ^ии  Right Front");		break;
                            case 0:		sprintf((char *)buf, "               ^и  Left Front");		break;
                        }
                        [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+1];
                    }
                }

                sprintf((char *)buf, "%u", pAudioInTermDesc->descChannelNames );
                [thisDevice addProperty:"String index for first logical channel:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                if ( !pAudioInTermDesc->descTermName )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioInTermDesc->descTermName );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioInTermDesc->descTermName );
                    }
                    [thisDevice addProperty:"Terminal Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                dumpIt ( ( UInt8 * )pAudioInTermDesc, thisDevice, 1 );
                break;

            case AC20S_CLOCK_SOURCE:
                pAudioClockSourceDesc = (Audio20ClockSourceDescriptorPtr)desc;
                sprintf((char *)buf, "%u", pAudioClockSourceDesc->descClockID );
                [thisDevice addProperty:"Clock ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

				switch( pAudioClockSourceDesc->descAttributes & 3 ) {
					case 0: sprintf((char *)buf, "External Clock" ); break;
					case 1: sprintf((char *)buf, "Internal Fixed Clock" ); break;
					case 2: sprintf((char *)buf, "Internal Variable Clock" ); break;
					case 3: sprintf((char *)buf, "Internal Programmable Clock" ); break;
				}
				if ( 4 & pAudioClockSourceDesc->descAttributes ) strcat( (char *) buf, " Sync'd to SOF" );
				[thisDevice addProperty:"Attributes:" withValue:buf atDepth:INTERFACE_LEVEL+1];

			
				sprintf((char *)buf, "%u", pAudioClockSourceDesc->descbmControls );
				[thisDevice addProperty:"Controls:" withValue:buf atDepth:INTERFACE_LEVEL+1];
  
				sprintf((char *)buf, "%u", pAudioClockSourceDesc->descAssocTermID );
				[thisDevice addProperty:"Associated Terminal:" withValue:buf atDepth:INTERFACE_LEVEL+1];
  
				if ( !pAudioClockSourceDesc->desciClockSourceName )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioClockSourceDesc->desciClockSourceName );
                }
				else
				{
					sprintf((char *)buf, "%u", pAudioClockSourceDesc->desciClockSourceName );
				}
				[thisDevice addProperty:"Clock Source Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                dumpIt ( ( UInt8 * )pAudioClockSourceDesc, thisDevice, 1 );

                break;
				
			case AC20S_CLOCK_SELECTOR:
				pAudioClockSelectorDesc = (Audio20ClockSelectorDescriptorPtr)desc;
				
				sprintf( (char *)buf, "%u", pAudioClockSelectorDesc->descClockID );
                [thisDevice addProperty:"Clock ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				bufPtr = pAudioClockSelectorDesc->descClockPID;
				for( i = 0; i < pAudioClockSelectorDesc->descNumPins; i++ )
                {
                    sprintf( (char *)buf2,	"Clock Source ID Pin[%d]:", i+1 );
                    sprintf( (char *)buf,	"%u", pAudioClockSelectorDesc->descClockPID[i] );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
					bufPtr++;
                }
				
				sprintf( (char *)buf, "%u", *bufPtr );
				[thisDevice addProperty:"Controls:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				bufPtr++;
				if ( !*bufPtr )
                {
                    sprintf( (char *)buf, "%u [NONE]", *bufPtr );
                }
				else
				{
					sprintf( (char *)buf, "%u", *bufPtr );
				}
				[thisDevice addProperty:"Clock Selector Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                dumpIt ( ( UInt8 * )pAudioClockSelectorDesc, thisDevice, 1 );
				break;
				
			case AC20S_CLOCK_MULTIPLIER:	
				pAudioClockMultiplierDesc = (Audio20ClockMultiplierDescriptorPtr)desc;
				
                sprintf( (char *)buf, "%u", pAudioClockMultiplierDesc->descClockID );
                [thisDevice addProperty:"Clock ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf( (char *)buf, "%u", pAudioClockMultiplierDesc->descClockSourceID );
                [thisDevice addProperty:"Clock Source ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf( (char *)buf, "%u", pAudioClockMultiplierDesc->descbmControls );
				[thisDevice addProperty:"Controls:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if ( !pAudioClockMultiplierDesc->desciClockMultiplierName )
                {
                    sprintf( (char *)buf, "%u [NONE]", pAudioClockMultiplierDesc->desciClockMultiplierName );
                }
				else
				{
					sprintf( (char *)buf, "%u", pAudioClockMultiplierDesc->desciClockMultiplierName );
				}
				[thisDevice addProperty:"Clock Multiplier Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                dumpIt ( ( UInt8 * )pAudioClockMultiplierDesc, thisDevice, 1 );
				break;
				
            case ACS_OUTPUT_TERMINAL:
                pAudioOutTermDesc = (Audio20CtrlOutTermDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_OUTPUT_TERMINAL" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioOutTermDesc->descTermID );
                [thisDevice addProperty:"Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // To get output terminal types refer to USB PDF files for Termt10.pdf p. 8.
                switch ( Swap16(&pAudioOutTermDesc->descTermType) )
                {
                    case 0x100: s="USB Undefined"; break;
                    case 0x101: s="USB Isochronous Stream"; break;
                    case 0x1ff: s="USB vendor specific"; break;

                    case 0x0300: s="Output Undefined"; break;
                    case 0x0301: s="Speaker"; break;
                    case 0x0302: s="Headphones"; break;
                    case 0x0303: s="Head Mounted Display Audio"; break;
                    case 0x0304: s="Desktop speaker"; break;
                    case 0x0305: s="Room speaker"; break;
                    case 0x0306: s="Communication speaker"; break;
                    case 0x0307: s="Low frequency effects speaker"; break;

                    case 0x400: s="Bi-directional Terminal, Undefined"; break;
                    case 0x401: s="Bi-directional Handset"; break;
                    case 0x402: s="Bi-directional Headset"; break;
                    case 0x403: s="Bi-directional Speakerphone (no echo reduction)"; break;
                    case 0x404: s="Bi-directional Speakerphone (echo supression)"; break;
                    case 0x405: s="Bi-directional Speakerphone (echo canceling)"; break;

                    case 0x500: s="Telephony Terminal, Undefined"; break;
                    case 0x501: s="Telephony Phoneline"; break;
                    case 0x502: s="Telephony Telephone"; break;
                    case 0x503: s="Telephony Down Line Phone"; break;

                    case 0x600: s="External Undefined"; break;
                    case 0x601: s="Analog connector"; break;
                    case 0x602: s="Digital audio connector"; break;
                    case 0x603: s="Line connector"; break;
                    case 0x604: s="Legacy audio connector"; break;
                    case 0x605: s="S/PDIF interface"; break;
                    case 0x606: s="1394 DA stream"; break;
                    case 0x607: s="1394 DV stream soundtrack"; break;

                    case 0x700: s="Embedded Terminal Undefined"; break;
                    case 0x701: s="Embedded Level Calibration Noise"; break;
                    case 0x702: s="Embedded Equalization Noise"; break;
                    case 0x704: s="Embedded Digital Audio Tape"; break;
                    case 0x705: s="Embedded Digital Compact Cassette"; break;
                    case 0x706: s="Embedded MiniDisk"; break;
                    case 0x707: s="Embedded Analog Tape"; break;
                    case 0x708: s="Phonograph"; break;
                    case 0x709: s="VCR Audio"; break;
                    case 0x70A: s="Video Disc Audio"; break;
                    case 0x70B: s="DVD Audio"; break;
                    case 0x70C: s="TV Tuner Audio"; break;
                    case 0x70D: s="Satellite Receiver Audio"; break;
                    case 0x70E: s="Cable Tuner Audio"; break;
                    case 0x70F: s="DSS Audio"; break;
                    case 0x710: s="Radio Receiver"; break;
                    case 0x711: s="Embedded AM/FM Radio Transmitter"; break;
                    case 0x712: s="Embedded Multi-track Recorder"; break;
                    case 0x713: s="Synthesizer"; break;
                    default: s="Invalid Output Terminal Type";
                }

                sprintf((char *)buf, 	"0x%x (%s)", pAudioOutTermDesc->descTermType, s );
                [thisDevice addProperty:"Output Terminal Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                if ( !pAudioOutTermDesc->descInTermID )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioOutTermDesc->descInTermID );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioOutTermDesc->descInTermID );
                    }
                    [thisDevice addProperty:"InTerminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];


                sprintf((char *)buf, "%u", pAudioOutTermDesc->descSourceID );
                [thisDevice addProperty:"Source ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];


                if ( !pAudioOutTermDesc->descTermName )
                {
                    sprintf((char *)buf, "%u [NONE]", pAudioOutTermDesc->descTermName );
                }
                    else
                    {
                        sprintf((char *)buf, "%u", pAudioOutTermDesc->descTermName );
                    }
                    [thisDevice addProperty:"Terminal Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                dumpIt ( ( UInt8 * )pAudioOutTermDesc, thisDevice, 1 );
                break;
            case ACS_MIXER_UNIT:
                pAudioMixerDesc = (AudioCtrlMixerDescriptorPtr)desc;
                sprintf((char *)buf, "ACS_MIXER_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioMixerDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pAudioMixerDesc->descNumPins );
                [thisDevice addProperty:"Number of pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // Add parsing for other pin IDs and channel configurations.
                for( i=0; i < pAudioMixerDesc->descNumPins; i++ )
                {

                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i+1);
                    sprintf((char *)buf,	"%u", pAudioMixerDesc->descSourcePID[i] );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                dumpIt ( ( UInt8 * )pAudioMixerDesc, thisDevice, 1 );
				break;

            case ACS_SELECTOR_UNIT:
                pAudioSelectorDesc = (AudioCtrlSelectorDescriptorPtr)desc;
                sprintf((char *)buf, "ACS_SELECTOR_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioSelectorDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pAudioSelectorDesc->descNumPins );
                [thisDevice addProperty:"Number of pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // Add parsing for other pin IDs and channel configurations.
                for( i=0; i < pAudioSelectorDesc->descNumPins; i++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf,	"%u", pAudioSelectorDesc->descSourcePID[i] );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                dumpIt ( ( UInt8 * )pAudioSelectorDesc, thisDevice, 1 );
                    break;

            case ACS_FEATURE_UNIT:
                pAudioFeatureDesc = (Audio20CtrlFeatureDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_FEATURE_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, 	"%u", pAudioFeatureDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pAudioFeatureDesc->descSourceID );
                [thisDevice addProperty:"Source ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // The feature descriptor length equals len=7+(ch+1)*n from Audio Class
                //    Devices p. 43).
                //
                // Algebra:
                //        len - 7 = (ch + 1)*n
                //        (len-7)/n = ch + 1
                //        ((len-7)/n) - 1 = ch
                //
                // So, ch = ((len-7)/n) - 1;
                //
				n = 4;
				ch = ((pAudioFeatureDesc->descLen - 6)/n) - 1;
                sprintf((char *)buf,	"%u", ch );
                [thisDevice addProperty:"Number of Channels (ch):" withValue:buf atDepth:INTERFACE_LEVEL+1];

                p=&pAudioFeatureDesc->descControls[0];
                for ( tempIndex = 0; tempIndex <= ch; tempIndex++ )
                {
                    buf[0] = 0;
					s = (char*)buf;
                    switch( tempIndex )
                    {
                        case 0:		strcat  (s, "Master Channel:......................... ( " );	break;
                        case 1:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 2:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 3:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 4:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 5:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 6:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 7:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 8:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        case 9:	sprintf (s, "Channel %d:............................. ( ", tempIndex );	break;
                        default:sprintf (s, "Channel %2d:............................ ( ", tempIndex );	break;
                    }

#define BM_BITS( str, x, sh ) if ( ( x & ( 3 << sh ) ) >> sh ) { \
							strcat (s, str ); switch( ( x & ( 3 << sh ) ) >> sh ) { \
							case 1: strcat(s, "RO " ); addedAttribute = true; break; \
							case 2: strcat(s, "Invalid " ); addedAttribute = true; break; \
							case 3: strcat(s, "RW " ); addedAttribute = true; break; } }
                    // The number of bytes for each field is indicated by descCtrlSize.
					addedAttribute = FALSE;
					BM_BITS( "Mute:", p[0], 0 );
					BM_BITS( "Volume:", p[0], 2 );
					BM_BITS( "Bass:", p[0], 4 );
					BM_BITS( "Mid:", p[0], 6 );
					BM_BITS( "Treble:", p[1], 0 );
					BM_BITS( "Graphic EQ:", p[1],2 );
					BM_BITS( "AGC:", p[1], 4 );
					BM_BITS( "Delay:", p[1], 6 );
					BM_BITS( "Bass Boost:", p[2], 0 );
					BM_BITS( "Loudness:", p[2], 2 );
					BM_BITS( "InputGain:", p[2], 4 );
					BM_BITS( "InputGainPad:", p[2], 6 );
					BM_BITS( "Phase Inverter:", p[3], 0 );
					BM_BITS( "UnderFlow:", p[3], 2 );
					BM_BITS( "OverFlow:", p[3], 4 );
					BM_BITS( "Reserved:", p[3], 6 );
                    p += n;
					
					// Destroy trailing comma or spaces by terminating string early if necessary
                    if (addedAttribute)
					{
					//	*(s + strlen(s) - 2) = 0;
                    }
					strcat (s, " )" );
					[thisDevice addProperty:" " withValue:s atDepth:INTERFACE_LEVEL+1];
                    // AddStringChild(item, buf);
                }

                    // p points to the next descriptor byte.
                    sprintf((char *)buf, "%u", *p );
                [thisDevice addProperty:"Feature Unit Name String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                dumpIt ( ( UInt8 * )pAudioFeatureDesc, thisDevice, 1 );
                break;
            case ACS_EXTENSION_UNIT:
                pAudioExtDesc = (Audio20CtrlExtDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_EXTENSION_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, "%u", pAudioExtDesc->descUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%x", Swap16(&pAudioExtDesc->descExtensionCode) );
                [thisDevice addProperty:"Vendor Ext. Code:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioExtDesc->descNumPins );
                [thisDevice addProperty:"Number of Input Pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                // The feature descriptor length equals len=13+descNumPins+bControlSize
                //    from Audio Class Devices p. 56.

                // Add parsing for other pin IDs and channel configurations.
                for( i=0; i < pAudioExtDesc->descNumPins; i++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf,	"%u", pAudioExtDesc->descSourcePID[i] );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                dumpIt ( ( UInt8 * )pAudioExtDesc, thisDevice, 1 );

                break;
            case ACS_PROCESSING_UNIT:
                pAudioProcDesc = (ac20ProcessingDescriptorPtr)desc;
                //sprintf((char *)buf, "ACS_PROCESSING_UNIT" ); // AddStringChild(item, buf);
                sprintf((char *)buf, "%u", pAudioProcDesc->bUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%04x", Swap16(&pAudioProcDesc->wProcessType) );
                [thisDevice addProperty:"Process Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioProcDesc->bNrPins );
                [thisDevice addProperty:"Number of Input Pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                for( srcIndex = 0, srcIdPtr = (UInt8*)&pAudioProcDesc->bNrPins; srcIndex < pAudioProcDesc->bNrPins; srcIndex++ )
                {
                    sprintf((char *)buf2, "SourceID(%u):", srcIndex);
                    sprintf((char *)buf, "%u", *srcIdPtr++ );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
                    pAudioProcContDesc = (ac20ProcessingDescriptorContPtr)srcIdPtr;
                sprintf((char *)buf, "%u", pAudioProcContDesc->bNrChannels );
                [thisDevice addProperty:"Number of Channels:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%04x", Swap16(&pAudioProcContDesc->wChannelConfig ));
                [thisDevice addProperty:"Channel Configuration:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioProcContDesc->iChannelNames );
                [thisDevice addProperty:"Channel Names:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "0x%04x", Swap16(&pAudioProcContDesc->bmControls ));
                [thisDevice addProperty:"Controls:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", pAudioProcContDesc->iProcessing );
                [thisDevice addProperty:"Process Unit Name:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                dumpIt ( ( UInt8 * )pAudioProcDesc, thisDevice, 1 );

                break;
				}
			}
			
            else            if ( [[thisDevice lastInterfaceClassInfo] subclassNum]==0x02 /*AudioStreaming*/ )
                switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
                {

                    case ACS_ASTREAM_GENERAL:
                        sprintf((char *)buf, "Audio Stream General" );
                        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL+1];

                        pAudioGeneralDesc = (CS20AS_InterfaceDescriptorPtr)desc;
                        sprintf((char *)buf, "%u", pAudioGeneralDesc->terminalID );
                        [thisDevice addProperty:"Endpoint Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u", pAudioGeneralDesc->formatType );
                        [thisDevice addProperty:"Format Type:" withValue:buf atDepth:INTERFACE_LEVEL+2];
						
						buf[ 0 ] = 0;
						UInt32 tempFormat = Swap32( & pAudioGeneralDesc->bmFormats );
						if ( 1 == pAudioGeneralDesc->formatType )
						{
							for ( tempIndex = 0; ( tempIndex < 32 ) && tempFormat; tempIndex++ )
							{
								char lilbuf[14] = { 0 };
								if ( tempFormat & 1 ) switch( tempIndex ) {
									case 0: strcat( ( char * ) buf, "PCM "); break;
									case 1: strcat( ( char * ) buf, "PCM8 "); break;
									case 2: strcat( ( char * ) buf, "Float "); break;
									case 3: strcat( ( char * ) buf, "ALAW "); break;
									case 4: strcat( ( char * ) buf, "uLAW "); break;
									case 31: strcat( ( char * ) buf, "RAW "); break;
									default: sprintf( lilbuf, "Reserved:%d ", tempIndex ); strcat( ( char * ) buf,  lilbuf ); break;
								}
								tempFormat >>= 1;
							}
						} else {
							sprintf((char *)buf, "%u", (unsigned int) Swap32( & pAudioGeneralDesc->bmFormats ) );
						}
                        [thisDevice addProperty:"Formats" withValue:buf atDepth:INTERFACE_LEVEL+2];

						sprintf((char *)buf, "%d", (unsigned int) pAudioGeneralDesc->numChannels );
                        [thisDevice addProperty:"Number of Channels" withValue:buf atDepth:INTERFACE_LEVEL+2];

						sprintf((char *)buf, "%u", (unsigned int) Swap32( & pAudioGeneralDesc->channelConfig ) );
                        [thisDevice addProperty:"Channel Configuration" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        dumpIt ( ( UInt8 * )pAudioGeneralDesc, thisDevice, 2 );

                        break;
                    case ACS_ASTREAM_TYPE:
                        sprintf((char *)buf, "Audio Stream Format Type Desc." );
                        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL+1];

                        pAudioFormatTypeDesc = (CS20AS_FormatTypeIDescPtr)desc;
                        sprintf((char *)buf, "%u", pAudioFormatTypeDesc->formatType );
                        [thisDevice addProperty:"Format Type:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u", pAudioFormatTypeDesc->slotSize );
                        [thisDevice addProperty:"Slot Size:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u", pAudioFormatTypeDesc->bitResolution );
                        [thisDevice addProperty:"Bit Resolution:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        dumpIt ( ( UInt8 * )pAudioFormatTypeDesc, thisDevice, 2 );
						break;
                    default:
                        sprintf((char *)buf, "AudioStreaming Subclass" );
                        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL+1];
	}
                    else if ( [[thisDevice lastInterfaceClassInfo] subclassNum]==0x03 /*MIDIStreaming*/ )
                        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
                        {
                        }

}
@end
