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

#import "DecodeHIDReport.h"


@implementation DecodeHIDReport

+(void)DecodeHIDReport:(UInt8 *)reportDesc forDevice:(UInt16)deviceNumber atDepth:(UInt16)depth reportLen:(UInt16)length forNode:(Node *)node
{
    UInt8 *			end = reportDesc + length;
    UInt8			size, type, tag;
    UInt32			usagePage = 0;
    UInt32			value=0;
    SInt32			svalue=0;
    static unsigned char	buf[350], tempbuf[350], bufvalue[350], tempbufvalue[350];
    int				i, indentLevel;
    Boolean			datahandled=false;
    Boolean			usagesigned=false;

    [self PrintKeyVal:"Parsed Report Descriptor:" val:"" forDevice:deviceNumber atDepth:depth forNode:node];
    indentLevel = 1;

    while (reportDesc < end)
    {
        size = UnpackReportSize(*reportDesc);
        if (size == 3) size = 4;	// 0 == 0 bytes, 1 == 1 bytes, 2 == 2 bytes, but 3 == 4 bytes

        type = UnpackReportType(*reportDesc);
        tag = UnpackReportTag(*reportDesc);
        reportDesc++;

        if (tag == kReport_TagLongItem)
        {
            size = *reportDesc++;
            tag = *reportDesc++;
        }


        // if we're small enough, load the value into a register (byte swaping)
        if (size <= 4)
        {
            value = 0;
            for (i = 0; i < size; i++)
                value += (*(reportDesc++)) << (i * 8);

            svalue = 0;
            switch (size)
            {
                case 1: svalue = (SInt8) value; break;
                case 2: svalue = (SInt16) value; break;

                    // if the top bit is set, then sign extend it and fall thru to 32bit case
                case 3: if (value & 0x00800000) value |= 0xFF000000; // no break
                case 4: svalue = (SInt32) value; break;
            }
        }

        // indent this line
        buf[0] = 0;
        bufvalue[0] = 0;
        for (i = 0; i < indentLevel; i++)
            strcat((char *)buf, "  ");


        // get the name of this tag, and do any specific data handling
        datahandled = false;
        switch (type)
        {
            case kReport_TypeMain:
                switch (tag)
                {
                    case kReport_TagInput:
                    case kReport_TagOutput:
                    case kReport_TagFeature:
                        switch (tag)
                        {
                            case kReport_TagInput: strcat((char *)buf, "Input..................."); break;
                            case kReport_TagOutput: strcat((char *)buf, "Output.................."); break;
                            case kReport_TagFeature: strcat((char *)buf, "Feature................."); break;
                        }

                        strcat((char *)bufvalue, (char *)"(");

                        strcat((char *)bufvalue, (value & kIO_Data_or_Constant) ? "Constant, " : "Data, ");

                        strcat((char *)bufvalue, (value & kIO_Array_or_Variable) ? "Variable, ": "Array, ");

                        strcat((char *)bufvalue, (value & kIO_Absolute_or_Relative) ? "Relative" : "Absolute");

                        if (((tag == kReport_TagInput) && (value & kIO_Array_or_Variable)) || tag != kReport_TagInput)
                        {	// these are only valid for variable inputs, and feature/output tags
                            strcat((char *)bufvalue, (value & kIO_NoWrap_or_Wrap) ? ", Wrap, " : ", No Wrap, ");

                            strcat((char *)bufvalue, (value & kIO_Linear_or_NonLinear) ? "Nonlinear, " : "Linear, ");

                            strcat((char *)bufvalue, (value & kIO_PreferredState_or_NoPreferred) ? "No Preferred, " : "Preferred State, ");

                            strcat((char *)bufvalue, (value & kIO_NoNullPosition_or_NullState) ? "Null State, " : "No Null Position, ");

                            if (tag != kReport_TagInput)
                                strcat((char *)bufvalue, (value & kIO_NonVolatile_or_Volatile) ? "Volatile, " : "Nonvolatile, ");

                            strcat((char *)bufvalue, (value & kIO_BitField_or_BufferedBytes) ? "Buffered bytes" : "Bitfield");
                        }

                            strcat((char *)bufvalue, (char *)")");

                        tempbuf[0] = 0;	// we don't want to add this again outside the switch
                        tempbufvalue[0] = 0;
                        datahandled = true;
                        break;


                    case kReport_TagCollection:
                        indentLevel++;

                        sprintf((char *)tempbuf, "Collection ");

                        strcat((char *)buf, (char *)tempbuf);

                        strcat((char *)buf, (char *)"(");
                        switch (value)
                        {
                            case kCollection_Physical: sprintf((char *)tempbuf, "Physical"); break;
                            case kCollection_Application:  sprintf((char *)tempbuf, "Application"); break;
                            case kCollection_Logical: sprintf((char *)tempbuf, "Logical"); break;
                        }
                            strcat((char *)buf, (char *)tempbuf);
                        strcat((char *)buf, (char *)")");

                        tempbuf[0] = 0;	// we don't want to add this again outside the switch
                        tempbufvalue[0] = 0;
                        datahandled = true;
                        break;

                    case kReport_TagEndCollection:
                        // recalc indentation, since we want this line to start earlier
                        indentLevel--;

                        buf[0] = 0;
                        for (i = 0; i < indentLevel; i++) {
                            strcat((char *)buf, "  ");
                        }

                            sprintf((char *)tempbuf, "End Collection ");


                        break;
                }
                break;

            case kReport_TypeGlobal:
                switch (tag)
                {
                    case kReport_TagUsagePage:
                        strcat((char *)buf, "Usage Page ");

                        usagesigned = true;
                        usagePage = value;
                        strcat((char *)bufvalue, (char *)"(");
                        switch (usagePage)
                        {
                            case kUsage_PageGenericDesktop: sprintf((char *)tempbufvalue, "Generic Desktop"); break;
                            case kUsage_PageSimulationControls: sprintf((char *)tempbufvalue, "Simulation Controls"); break;
                            case kUsage_PageVRControls: sprintf((char *)tempbufvalue, "VR Controls"); break;
                            case kUsage_PageSportControls: sprintf((char *)tempbufvalue, "Sports Controls"); break;
                            case kUsage_PageGameControls: sprintf((char *)tempbufvalue, "Game Controls"); break;
                            case kUsage_PageKeyboard:
                                sprintf((char *)tempbufvalue, "Keyboard/Keypad");
                                usagesigned = false;
                                break;

                            case kUsage_PageLED: sprintf((char *)tempbufvalue, "LED"); break;
                            case kUsage_PageButton: sprintf((char *)tempbufvalue, "Button"); break;
                            case kUsage_PageOrdinal: sprintf((char *)tempbufvalue, "Ordinal"); break;
                            case kUsage_PageTelephonyDevice: sprintf((char *)tempbufvalue, "Telephany Device"); break;
                            case kUsage_PageConsumer: sprintf((char *)tempbufvalue, "Consumer"); break;
                            case kUsage_PageDigitizers: sprintf((char *)tempbufvalue, "Digitizer"); break;
                            case kUsage_PageUnicode: sprintf((char *)tempbufvalue, "Unicode"); break;
                            case kUsage_PageAlphanumericDisplay: sprintf((char *)tempbufvalue, "Alphanumeric Display"); break;

                            case kUsage_PageMonitor: sprintf((char *)tempbufvalue, "Monitor"); break;
                            case kUsage_PageMonitorEnumeratedValues: sprintf((char *)tempbufvalue, "Monitor Enumerated Values"); break;
                            case kUsage_PageMonitorVirtualControl: sprintf((char *)tempbufvalue, "VESA Virtual Controls"); break;
                            case kUsage_PageMonitorReserved: sprintf((char *)tempbufvalue, "Monitor Class reserved"); break;
                            case kUsage_PagePowerDevice: sprintf((char *)tempbufvalue, "Power Device"); break;
                            case kUsage_PageBatterySystem: sprintf((char *)tempbufvalue, "Battery System"); break;
                            case kUsage_PowerClassReserved: sprintf((char *)tempbufvalue, "Power Class reserved"); break;
                            case kUsage_PowerClassReserved2: sprintf((char *)tempbufvalue, "Power Class reserved"); break;

                            default: sprintf((char *)tempbufvalue, "%ld", usagePage); break;
                        }

                            //strcat((char *)buf, (char *)tempbuf);
                            strcat((char *)bufvalue, (char *)tempbufvalue);
                        strcat((char *)bufvalue, (char *)")");
                        tempbuf[0] = 0;	// we don't want to add this again outside the switch
                        tempbufvalue[0] = 0;
                        datahandled = true;
                        break;

                    case kReport_TagLogicalMin: sprintf((char *)tempbuf,      "Logical Minimum......... "); break;
                    case kReport_TagLogicalMax: sprintf((char *)tempbuf,      "Logical Maximum......... "); break;
                    case kReport_TagPhysicalMin: sprintf((char *)tempbuf,     "Physical Minimum........ "); break;
                    case kReport_TagPhysicalMax: sprintf((char *)tempbuf,     "Physical Maximum........ "); break;
                    case kReport_TagUnitExponent: sprintf((char *)tempbuf,    "Unit Exponent........... "); break;
                    case kReport_TagUnit: sprintf((char *)tempbuf,            "Unit.................... "); break;
                    case kReport_TagReportSize: sprintf((char *)tempbuf,      "Report Size............. "); break;
                    case kReport_TagReportID: sprintf((char *)tempbuf,        "ReportID................ "); break;
                    case kReport_TagReportCount: sprintf((char *)tempbuf,     "Report Count............ "); break;
                    case kReport_TagPush: sprintf((char *)tempbuf,            "Push.................... "); break;
                    case kReport_TagPop: sprintf((char *)tempbuf,             "Pop..................... "); break;
                }
                break;

            case kReport_TypeLocal:
                switch (tag)
                {
                    case kReport_TagUsage:
                        sprintf((char *)tempbuf, "Usage ");
                        strcat((char *)buf, (char *)tempbuf);
                        if (usagePage == kUsage_PageGenericDesktop)
                        {
                            strcat((char *)buf, (char *)"(");
                            switch (value)
                            {
                                case kUsage_01_Pointer: sprintf((char *)tempbuf, "Pointer"); break;
                                case kUsage_01_Mouse: sprintf((char *)tempbuf, "Mouse"); break;
                                case kUsage_01_Joystick: sprintf((char *)tempbuf, "Joystick"); break;
                                case kUsage_01_GamePad: sprintf((char *)tempbuf, "GamePad"); break;
                                case kUsage_01_Keyboard: sprintf((char *)tempbuf, "Keyboard"); break;
                                case kUsage_01_Keypad: sprintf((char *)tempbuf, "Keypad"); break;

                                case kUsage_01_X: sprintf((char *)tempbuf, "X"); break;
                                case kUsage_01_Y: sprintf((char *)tempbuf, "Y"); break;
                                case kUsage_01_Z: sprintf((char *)tempbuf, "Z"); break;
                                case kUsage_01_Rx: sprintf((char *)tempbuf, "Rx"); break;
                                case kUsage_01_Ry: sprintf((char *)tempbuf, "Ry"); break;
                                case kUsage_01_Rz: sprintf((char *)tempbuf, "Rz"); break;
                                case kUsage_01_Slider: sprintf((char *)tempbuf, "Slider"); break;
                                case kUsage_01_Dial: sprintf((char *)tempbuf, "Dial"); break;
                                case kUsage_01_Wheel: sprintf((char *)tempbuf, "Wheel"); break;
                                case kUsage_01_HatSwitch: sprintf((char *)tempbuf, "Hat Switch"); break;
                                case kUsage_01_CountedBuffer: sprintf((char *)tempbuf, "Counted Buffer"); break;
                                case kUsage_01_ByteCount: sprintf((char *)tempbuf, "Byte Count"); break;
                                case kUsage_01_MotionWakeup: sprintf((char *)tempbuf, "Motion Wakeup"); break;

                                case kUsage_01_Vx: sprintf((char *)tempbuf, "Vx"); break;
                                case kUsage_01_Vy: sprintf((char *)tempbuf, "Vy"); break;
                                case kUsage_01_Vz: sprintf((char *)tempbuf, "Vz"); break;
                                case kUsage_01_Vbrx: sprintf((char *)tempbuf, "Vbrx"); break;
                                case kUsage_01_Vbry: sprintf((char *)tempbuf, "Vbry"); break;
                                case kUsage_01_Vbrz: sprintf((char *)tempbuf, "Vbrz"); break;
                                case kUsage_01_Vno: sprintf((char *)tempbuf, "Vno"); break;

                                case kUsage_01_SystemControl: sprintf((char *)tempbuf, "System Control"); break;
                                case kUsage_01_SystemPowerDown: sprintf((char *)tempbuf, "System Power Down"); break;
                                case kUsage_01_SystemSleep: sprintf((char *)tempbuf, "System Sleep"); break;
                                case kUsage_01_SystemWakeup: sprintf((char *)tempbuf, "System Wakeup"); break;
                                case kUsage_01_SystemContextMenu: sprintf((char *)tempbuf, "System Context Menu"); break;
                                case kUsage_01_SystemMainMenu: sprintf((char *)tempbuf, "System Main Menu"); break;
                                case kUsage_01_SystemAppMenu: sprintf((char *)tempbuf, "System App Menu"); break;
                                case kUsage_01_SystemMenuHelp: sprintf((char *)tempbuf, "System Menu Help"); break;
                                case kUsage_01_SystemMenuExit: sprintf((char *)tempbuf, "System Menu Exit"); break;
                                case kUsage_01_SystemMenuSelect: sprintf((char *)tempbuf, "System Menu Select"); break;
                                case kUsage_01_SystemMenuRight: sprintf((char *)tempbuf, "System Menu Right"); break;
                                case kUsage_01_SystemMenuLeft: sprintf((char *)tempbuf, "System Menu Left"); break;
                                case kUsage_01_SystemMenuUp: sprintf((char *)tempbuf, "System Menu Up"); break;
                                case kUsage_01_SystemMenuDown: sprintf((char *)tempbuf, "System Menu Down"); break;

                                default: sprintf((char *)tempbuf, "%d (0x%x)", (int)value, (unsigned int)value); break;
                            }
                            strcat((char *)tempbuf, (char *)")");
                        }
                            else
                            {
                                sprintf((char *)tempbuf, "%d (0x%x)", (int)value, (unsigned int)value);
                            }
                            strcat((char *)buf, (char *)tempbuf);
                        tempbuf[0] = 0;	// we don't want to add this again outside the switch
                        tempbufvalue[0] = 0;
                        datahandled = true;
                        break;

                    case kReport_TagUsageMin: sprintf((char *)tempbuf,        "Usage Minimum........... "); break;
                    case kReport_TagUsageMax: sprintf((char *)tempbuf,        "Usage Maximum........... "); break;
                    case kReport_TagDesignatorIndex: sprintf((char *)tempbuf, "Designator Index........ "); break;
                    case kReport_TagDesignatorMin: sprintf((char *)tempbuf,   "Designator Minumum...... "); break;
                    case kReport_TagDesignatorMax: sprintf((char *)tempbuf,   "Designator Maximum...... "); break;
                    case kReport_TagStringIndex: sprintf((char *)tempbuf,     "String Index............ "); break;
                    case kReport_TagStringMin: sprintf((char *)tempbuf,       "String Minimum.......... "); break;
                    case kReport_TagStringMax: sprintf((char *)tempbuf,       "String Maximum.......... "); break;
                    case kReport_TagSetDelimiter: sprintf((char *)tempbuf,    "Set Delimiter........... "); break;
                }
                break;

            case kReport_TypeReserved:
                sprintf((char *)tempbuf, "Reserved "); break;
                break;
        }

        // actually put in the data from the switch -- why not just strcat there??
        strcat((char *)buf, (char *)tempbuf);

        // if we didn't handle the data before, print in generic fashion
        if (!datahandled && size)
        {
            strcat((char *)bufvalue, (char *)"(");
            if (size <= 4)
            {
                if (usagesigned)
                {
                    sprintf((char *)tempbufvalue, "%ld", (SInt32)svalue);
                }
                else
                {
                    sprintf((char *)tempbufvalue, "%lu", (UInt32)value);
                }
                strcat((char *)bufvalue, (char *)tempbufvalue);
            }
            else
                for (i = 0; i < size; i++)
                {
                    sprintf((char *)tempbufvalue, "%02X ", *(reportDesc++));
                    strcat((char *)bufvalue, (char *)tempbufvalue);
                }
                    strcat((char *)bufvalue, (char *)") ");
        }


        // finally add the info
        strcat((char *)bufvalue, " "); // in case bufvalue was empty, add a blank space


        // this juggling is because the End Collection tags were not nested deep enough in the OutlineView.

        if (tag == kReport_TagEndCollection) {
            [self PrintKeyVal:buf val:bufvalue forDevice:deviceNumber atDepth:depth+indentLevel+1 forNode:node];
        }
        else
            [self PrintKeyVal:buf val:bufvalue forDevice:deviceNumber atDepth:depth+indentLevel forNode:node];
    }
}

@end
