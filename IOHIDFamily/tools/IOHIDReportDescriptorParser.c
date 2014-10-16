//
//  IOHIDReportDescriptorParser.c
//  IOHIDFamily
//
//  Created by Rob Yepez on 2/23/13.
//
//

#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include "IOHIDReportDescriptorParser.h"

#define    UnpackReportSize(packedByte)    ((packedByte) & 0x03)
#define    UnpackReportType(packedByte)    (((packedByte) & 0x0C) >> 2)
#define    UnpackReportTag(packedByte)    (((packedByte) & 0xF0) >> 4)

enum
{
    kReport_TypeMain            = 0,
    kReport_TypeGlobal            = 1,
    kReport_TypeLocal            = 2,
    kReport_TypeReserved        = 3,
    
    kReport_TagLongItem            = 0x0F,
    
    // main items
    kReport_TagInput            = 0x08,
    kReport_TagOutput            = 0x09,
    kReport_TagFeature            = 0x0B,
    kReport_TagCollection        = 0x0A,
    kReport_TagEndCollection    = 0x0C,
    
    // global items
    kReport_TagUsagePage        = 0x00,
    kReport_TagLogicalMin        = 0x01,
    kReport_TagLogicalMax        = 0x02,
    kReport_TagPhysicalMin        = 0x03,
    kReport_TagPhysicalMax        = 0x04,
    kReport_TagUnitExponent        = 0x05,
    kReport_TagUnit                = 0x06,
    kReport_TagReportSize        = 0x07,
    kReport_TagReportID            = 0x08,
    kReport_TagReportCount        = 0x09,
    kReport_TagPush                = 0x0A,
    kReport_TagPop                = 0x0B,
    
    // local items
    kReport_TagUsage            = 0x00,
    kReport_TagUsageMin            = 0x01,
    kReport_TagUsageMax            = 0x02,
    kReport_TagDesignatorIndex    = 0x03,
    kReport_TagDesignatorMin    = 0x04,
    kReport_TagDesignatorMax    = 0x05,
    kReport_TagStringIndex        = 0x07,
    kReport_TagStringMin        = 0x08,
    kReport_TagStringMax        = 0x09,
    kReport_TagSetDelimiter        = 0x0A
};

// Collection constants
enum
{
    kCollection_Physical        = 0x00,
    kCollection_Application        = 0x01,
    kCollection_Logical            = 0x02
};

// I/O constants (used for Input/Output/Feature tags)
enum
{
    kIO_Data_or_Constant                = 0x0001,
    kIO_Array_or_Variable                = 0x0002,
    kIO_Absolute_or_Relative            = 0x0004,
    kIO_NoWrap_or_Wrap                    = 0x0008,
    kIO_Linear_or_NonLinear                = 0x0010,
    kIO_PreferredState_or_NoPreferred    = 0x0020,
    kIO_NoNullPosition_or_NullState        = 0x0040,
    kIO_NonVolatile_or_Volatile            = 0x0080,        // reserved for Input
    kIO_BitField_or_BufferedBytes        = 0x0100
};


static void PrintAtIndentLevel(unsigned int level, const char * format, ...)
{
    va_list     ap;
    
    for (unsigned int i = 0; i < level; i++) {
        fputs("    ", stdout);
    }
    
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

static void PrintBytesAtIndentLevel(unsigned int level, const uint8_t * data, uint32_t length)
{
    uint32_t index;
    
    for ( index=0; index<length; index+=16 ) {
        uint32_t innerIndex;
        uint32_t innerLength = length-index;
        if ( innerLength > 16 )
            innerLength = 16;
        
        PrintAtIndentLevel(level, "%08X: ", index);
        for ( innerIndex=0; innerIndex<innerLength; innerIndex++)
            printf("%02X ", data[index+innerIndex]);
        
        printf("\n");
    }
}

void PrintHIDDescriptor(const uint8_t *reportDesc, uint32_t length)
{
    const uint8_t *         end = reportDesc + length;
    uint8_t                 size, type, tag;
    uint32_t                usagePage = 0;
    uint32_t                value=0;
    int32_t                 svalue=0;
    static unsigned char    buf[350], tempbuf[350], bufvalue[350], tempbufvalue[350];
    int                     i, indentLevel;
    int                     datahandled=0;
    int                     itemsigned=0;
    int                     len;
    
    printf("\n");
    printf("Raw HID Descriptor:\n");
    printf("---------------------------------------------------------\n");
    
    PrintBytesAtIndentLevel(0, reportDesc, length);
    
    printf("\n");
    printf("Parsed HID Descriptor:\n");
    printf("---------------------------------------------------------\n");
    indentLevel = 0;
    while (reportDesc < end)
    {
        int padLevel = 7;
        
        buf[0] = 0;
        bufvalue[0] = 0;
        size = UnpackReportSize(*reportDesc);
        if (size == 3) size = 4;    // 0 == 0 bytes, 1 == 1 bytes, 2 == 2 bytes, but 3 == 4 bytes
        
        type = UnpackReportType(*reportDesc);
        tag = UnpackReportTag(*reportDesc);

        sprintf((char *)tempbuf, "0x%02X, ", *(reportDesc++));
        strcat((char *)buf, (char *)tempbuf);
        padLevel--;
        
        if (tag == kReport_TagLongItem)
        {
            size = *reportDesc;
            sprintf((char *)tempbuf, "0x%02X, ", *(reportDesc++));
            strcat((char *)buf, (char *)tempbuf);
            tag = *reportDesc;
            sprintf((char *)tempbuf, "0x%02X, ", *(reportDesc++));
            strcat((char *)buf, (char *)tempbuf);
            
            padLevel -= 2;
        }
        
        
        // if we're small enough, load the value into a register (byte swaping)
        if (size <= 4)
        {
            value = 0;
            for (i = 0; i < size; i++) {
                value += (*(reportDesc)) << (i * 8);
                sprintf((char *)tempbuf, "0x%02X, ", *(reportDesc++));
                strcat((char *)buf, (char *)tempbuf);
                padLevel--;
            }
            
            svalue = 0;
            switch (size)
            {
                case 1: svalue = (int8_t) value; break;
                case 2: svalue = (int16_t) value; break;
                    
                    // if the top bit is set, then sign extend it and fall thru to 32bit case
                case 3: if (value & 0x00800000) value |= 0xFF000000; // no break
                case 4: svalue = (int32_t) value; break;
            }
        }
        
        // pad it a bit
        for (i = 0; i < padLevel; i++)
            strcat((char *)buf, "      ");
        
        strcat((char *)buf, "// ");

        // indent this line
        for (i = 0; i < indentLevel; i++)
            strcat((char *)buf, "  ");
        
        
        // get the name of this tag, and do any specific data handling
        datahandled = 0;
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
                    
                    if ( (value & kIO_Data_or_Constant) == 0 ) {
                    
                        strcat((char *)bufvalue, (value & kIO_Array_or_Variable) ? "Variable, ": "Array, ");
                        
                        strcat((char *)bufvalue, (value & kIO_Absolute_or_Relative) ? "Relative, " : "Absolute, ");
                        
                        if (((tag == kReport_TagInput) && (value & kIO_Array_or_Variable)) || tag != kReport_TagInput)
                        {
    #if VERBOSE
                            // these are only valid for variable inputs, and feature/output tags
                            strcat((char *)bufvalue, (value & kIO_NoWrap_or_Wrap) ? "Wrap, " : "No Wrap, ");
                            strcat((char *)bufvalue, (value & kIO_Linear_or_NonLinear) ? "Nonlinear, " : "Linear, ");
                            strcat((char *)bufvalue, (value & kIO_PreferredState_or_NoPreferred) ? "No Preferred, " : "Preferred State, ");
                            strcat((char *)bufvalue, (value & kIO_NoNullPosition_or_NullState) ? "Null State, " : "No Null Position, ");
                            
                            if (tag != kReport_TagInput)
                                strcat((char *)bufvalue, (value & kIO_NonVolatile_or_Volatile) ? "Volatile, " : "Nonvolatile, ");
                            strcat((char *)bufvalue, (value & kIO_BitField_or_BufferedBytes) ? "Buffered bytes" : "Bitfield");
    #else
                            // these are only valid for variable inputs, and feature/output tags
                            strcat((char *)bufvalue, (value & kIO_NoWrap_or_Wrap) ? "Wrap, " : "");
                            strcat((char *)bufvalue, (value & kIO_Linear_or_NonLinear) ? "Nonlinear, " : "");
                            strcat((char *)bufvalue, (value & kIO_PreferredState_or_NoPreferred) ? "No Preferred, " : "");
                            strcat((char *)bufvalue, (value & kIO_NoNullPosition_or_NullState) ? "Null State, " : "");
                            
                            if (tag != kReport_TagInput)
                                strcat((char *)bufvalue, (value & kIO_NonVolatile_or_Volatile) ? "Volatile, " : "");
                            strcat((char *)bufvalue, (value & kIO_BitField_or_BufferedBytes) ? "Buffered bytes" : "");
    #endif
                        }
                    }
                    
                    len = strlen((char *)bufvalue);
                    if ( strcmp((const char *)&bufvalue[len-2], ", ") == 0 )
                        bufvalue[len-2]=0;
                    strcat((char *)bufvalue, (char *)")");
                    
                    tempbuf[0] = 0;    // we don't want to add this again outside the switch
                    tempbufvalue[0] = 0;
                    datahandled = 1;
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
                    
                    tempbuf[0] = 0;    // we don't want to add this again outside the switch
                    tempbufvalue[0] = 0;
                    datahandled = 1;
                    break;
                    
                case kReport_TagEndCollection:
                {
                    // recalc indentation, since we want this line to start earlier
                    
                    len = strlen((char *)buf);
                    
                    if (indentLevel-- && len > 2) {
                        buf[len-2]=0;
                    }
                    
                    sprintf((char *)tempbuf, "End Collection ");
                    
                }
                    break;
            }
                break;
                
            case kReport_TypeGlobal:
                switch (tag)
            {
                case kReport_TagUsagePage:
                    strcat((char *)buf, "Usage Page ");
                    usagePage = value;
                    strcat((char *)bufvalue, (char *)"(");
                    switch (usagePage)
                    {
                        case kHIDPage_GenericDesktop: sprintf((char *)tempbufvalue, "Generic Desktop"); break;
                        case kHIDPage_Simulation: sprintf((char *)tempbufvalue, "Simulation Controls"); break;
                        case kHIDPage_VR: sprintf((char *)tempbufvalue, "VR Controls"); break;
                        case kHIDPage_Sport: sprintf((char *)tempbufvalue, "Sports Controls"); break;
                        case kHIDPage_Game: sprintf((char *)tempbufvalue, "Game Controls"); break;
                        case kHIDPage_KeyboardOrKeypad: sprintf((char *)tempbufvalue, "Keyboard/Keypad"); break;
                        case kHIDPage_LEDs: sprintf((char *)tempbufvalue, "LED"); break;
                        case kHIDPage_Button: sprintf((char *)tempbufvalue, "Button"); break;
                        case kHIDPage_Ordinal: sprintf((char *)tempbufvalue, "Ordinal"); break;
                        case kHIDPage_Telephony: sprintf((char *)tempbufvalue, "Telephony Device"); break;
                        case kHIDPage_Consumer: sprintf((char *)tempbufvalue, "Consumer"); break;
                        case kHIDPage_Digitizer: sprintf((char *)tempbufvalue, "Digitizer"); break;
                        case kHIDPage_PID: sprintf((char *)tempbufvalue, "PID"); break;
                        case kHIDPage_Unicode: sprintf((char *)tempbufvalue, "Unicode"); break;
                        case kHIDPage_AlphanumericDisplay: sprintf((char *)tempbufvalue, "Alphanumeric Display"); break;
                        case kHIDPage_Monitor: sprintf((char *)tempbufvalue, "Monitor"); break;
                        case kHIDPage_MonitorEnumerated: sprintf((char *)tempbufvalue, "Monitor Enumerated Values"); break;
                        case kHIDPage_MonitorVirtual: sprintf((char *)tempbufvalue, "VESA Virtual Controls"); break;
                        case kHIDPage_MonitorReserved: sprintf((char *)tempbufvalue, "Monitor Class reserved"); break;
                        case kHIDPage_PowerDevice: sprintf((char *)tempbufvalue, "Power Device"); break;
                        case kHIDPage_BatterySystem: sprintf((char *)tempbufvalue, "Battery System"); break;
                        case kHIDPage_PowerReserved: sprintf((char *)tempbufvalue, "Power Class reserved"); break;
                        case kHIDPage_PowerReserved2: sprintf((char *)tempbufvalue, "Power Class reserved"); break;
                        case 0xff: sprintf((char *)tempbufvalue, "Vendor Defined"); break;
                            
                        default: sprintf((char *)tempbufvalue, "%u", usagePage); break;
                    }
                    
                    //strcat((char *)buf, (char *)tempbuf);
                    strcat((char *)bufvalue, (char *)tempbufvalue);
                    strcat((char *)bufvalue, (char *)")");
                    tempbuf[0] = 0;    // we don't want to add this again outside the switch
                    tempbufvalue[0] = 0;
                    datahandled = 1;
                    break;
                    
                case kReport_TagLogicalMin: sprintf((char *)tempbuf,      "Logical Minimum......... "); itemsigned=1; break;
                case kReport_TagLogicalMax: sprintf((char *)tempbuf,      "Logical Maximum......... "); itemsigned=1; break;
                case kReport_TagPhysicalMin: sprintf((char *)tempbuf,     "Physical Minimum........ "); itemsigned=1; break;
                case kReport_TagPhysicalMax: sprintf((char *)tempbuf,     "Physical Maximum........ "); itemsigned=1; break;
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
                    if (usagePage == kHIDPage_GenericDesktop)
                    {
                        strcat((char *)buf, (char *)"(");
                        switch (value)
                        {
                            case kHIDUsage_GD_Pointer: sprintf((char *)tempbuf, "Pointer"); break;
                            case kHIDUsage_GD_Mouse: sprintf((char *)tempbuf, "Mouse"); break;
                            case kHIDUsage_GD_Joystick: sprintf((char *)tempbuf, "Joystick"); break;
                            case kHIDUsage_GD_GamePad: sprintf((char *)tempbuf, "GamePad"); break;
                            case kHIDUsage_GD_Keyboard: sprintf((char *)tempbuf, "Keyboard"); break;
                            case kHIDUsage_GD_Keypad: sprintf((char *)tempbuf, "Keypad"); break;
                            case kHIDUsage_GD_MultiAxisController:  sprintf((char *)tempbuf, "MultiAxisController"); break;
                                
                            case kHIDUsage_GD_X: sprintf((char *)tempbuf, "X"); break;
                            case kHIDUsage_GD_Y: sprintf((char *)tempbuf, "Y"); break;
                            case kHIDUsage_GD_Z: sprintf((char *)tempbuf, "Z"); break;
                            case kHIDUsage_GD_Rx: sprintf((char *)tempbuf, "Rx"); break;
                            case kHIDUsage_GD_Ry: sprintf((char *)tempbuf, "Ry"); break;
                            case kHIDUsage_GD_Rz: sprintf((char *)tempbuf, "Rz"); break;
                            case kHIDUsage_GD_Slider: sprintf((char *)tempbuf, "Slider"); break;
                            case kHIDUsage_GD_Dial: sprintf((char *)tempbuf, "Dial"); break;
                            case kHIDUsage_GD_Wheel: sprintf((char *)tempbuf, "Wheel"); break;
                            case kHIDUsage_GD_Hatswitch: sprintf((char *)tempbuf, "Hat Switch"); break;
                            case kHIDUsage_GD_CountedBuffer: sprintf((char *)tempbuf, "Counted Buffer"); break;
                            case kHIDUsage_GD_ByteCount: sprintf((char *)tempbuf, "Byte Count"); break;
                            case kHIDUsage_GD_MotionWakeup: sprintf((char *)tempbuf, "Motion Wakeup"); break;
                                
                            case kHIDUsage_GD_Vx: sprintf((char *)tempbuf, "Vx"); break;
                            case kHIDUsage_GD_Vy: sprintf((char *)tempbuf, "Vy"); break;
                            case kHIDUsage_GD_Vz: sprintf((char *)tempbuf, "Vz"); break;
                            case kHIDUsage_GD_Vbrx: sprintf((char *)tempbuf, "Vbrx"); break;
                            case kHIDUsage_GD_Vbry: sprintf((char *)tempbuf, "Vbry"); break;
                            case kHIDUsage_GD_Vbrz: sprintf((char *)tempbuf, "Vbrz"); break;
                            case kHIDUsage_GD_Vno: sprintf((char *)tempbuf, "Vno"); break;
                                
                            case kHIDUsage_GD_SystemControl: sprintf((char *)tempbuf, "System Control"); break;
                            case kHIDUsage_GD_SystemPowerDown: sprintf((char *)tempbuf, "System Power Down"); break;
                            case kHIDUsage_GD_SystemSleep: sprintf((char *)tempbuf, "System Sleep"); break;
                            case kHIDUsage_GD_SystemWakeUp: sprintf((char *)tempbuf, "System Wakeup"); break;
                            case kHIDUsage_GD_SystemContextMenu: sprintf((char *)tempbuf, "System Context Menu"); break;
                            case kHIDUsage_GD_SystemMainMenu: sprintf((char *)tempbuf, "System Main Menu"); break;
                            case kHIDUsage_GD_SystemAppMenu: sprintf((char *)tempbuf, "System App Menu"); break;
                            case kHIDUsage_GD_SystemMenuHelp: sprintf((char *)tempbuf, "System Menu Help"); break;
                            case kHIDUsage_GD_SystemMenuExit: sprintf((char *)tempbuf, "System Menu Exit"); break;
                            case kHIDUsage_GD_SystemMenuSelect: sprintf((char *)tempbuf, "System Menu Select"); break;
                            case kHIDUsage_GD_SystemMenuRight: sprintf((char *)tempbuf, "System Menu Right"); break;
                            case kHIDUsage_GD_SystemMenuLeft: sprintf((char *)tempbuf, "System Menu Left"); break;
                            case kHIDUsage_GD_SystemMenuUp: sprintf((char *)tempbuf, "System Menu Up"); break;
                            case kHIDUsage_GD_SystemMenuDown: sprintf((char *)tempbuf, "System Menu Down"); break;
                                
                            default: sprintf((char *)tempbuf, "%d (0x%x)", (int)value, (unsigned int)value); break;
                        }
                        strcat((char *)tempbuf, (char *)")");
                    }
                    else if (usagePage == kHIDPage_Digitizer)
                    {
                        strcat((char *)buf, (char *)"(");
                        switch (value)
                        {
                            case kHIDUsage_Dig_Digitizer: sprintf((char *)tempbuf, "Digitizer"); break;
                            case kHIDUsage_Dig_Pen: sprintf((char *)tempbuf, "Pen"); break;
                            case kHIDUsage_Dig_LightPen: sprintf((char *)tempbuf, "Light Pen"); break;
                            case kHIDUsage_Dig_TouchScreen: sprintf((char *)tempbuf, "Touch Screen"); break;
                            case kHIDUsage_Dig_TouchPad: sprintf((char *)tempbuf, "Touch Pad"); break;
                            case kHIDUsage_Dig_WhiteBoard: sprintf((char *)tempbuf, "White Board"); break;
                            case kHIDUsage_Dig_CoordinateMeasuringMachine: sprintf((char *)tempbuf, "Coordinate Measuring Machine"); break;
                            case kHIDUsage_Dig_3DDigitizer: sprintf((char *)tempbuf, "3D Digitizer"); break;
                            case kHIDUsage_Dig_StereoPlotter: sprintf((char *)tempbuf, "Stereo Plotter"); break;
                            case kHIDUsage_Dig_ArticulatedArm: sprintf((char *)tempbuf, "Articulated Arm"); break;
                            case kHIDUsage_Dig_Armature: sprintf((char *)tempbuf, "Armature"); break;
                            case kHIDUsage_Dig_MultiplePointDigitizer: sprintf((char *)tempbuf, "Multi Point Digitizer"); break;
                            case kHIDUsage_Dig_FreeSpaceWand: sprintf((char *)tempbuf, "Free Space Wand"); break;
                            case kHIDUsage_Dig_DeviceConfiguration: sprintf((char *)tempbuf, "Device Configuration"); break;
                            case kHIDUsage_Dig_Stylus: sprintf((char *)tempbuf, "Stylus"); break;
                            case kHIDUsage_Dig_Puck: sprintf((char *)tempbuf, "Puck"); break;
                            case kHIDUsage_Dig_Finger: sprintf((char *)tempbuf, "Finger"); break;
                            case kHIDUsage_Dig_DeviceSettings: sprintf((char *)tempbuf, "Device Settings"); break;
                            case kHIDUsage_Dig_GestureCharacter: sprintf((char *)tempbuf, "Gesture Character"); break;
                            case kHIDUsage_Dig_TipPressure: sprintf((char *)tempbuf, "Tip Pressure"); break;
                            case kHIDUsage_Dig_BarrelPressure: sprintf((char *)tempbuf, "Barrel Pressure"); break;
                            case kHIDUsage_Dig_InRange: sprintf((char *)tempbuf, "In Range"); break;
                            case kHIDUsage_Dig_Touch: sprintf((char *)tempbuf, "Touch"); break;
                            case kHIDUsage_Dig_Untouch: sprintf((char *)tempbuf, "Untouch"); break;
                            case kHIDUsage_Dig_Tap: sprintf((char *)tempbuf, "Tap"); break;
                            case kHIDUsage_Dig_Quality: sprintf((char *)tempbuf, "Quality"); break;
                            case kHIDUsage_Dig_DataValid: sprintf((char *)tempbuf, "Data Valid"); break;
                            case kHIDUsage_Dig_TransducerIndex: sprintf((char *)tempbuf, "Transducer Index"); break;
                            case kHIDUsage_Dig_TabletFunctionKeys: sprintf((char *)tempbuf, "Tablet Function Keys"); break;
                            case kHIDUsage_Dig_ProgramChangeKeys: sprintf((char *)tempbuf, "Program Change Buttons"); break;
                            case kHIDUsage_Dig_BatteryStrength: sprintf((char *)tempbuf, "Battery Strength"); break;
                            case kHIDUsage_Dig_Invert: sprintf((char *)tempbuf, "Invert"); break;
                            case kHIDUsage_Dig_XTilt: sprintf((char *)tempbuf, "X Tilt"); break;
                            case kHIDUsage_Dig_YTilt: sprintf((char *)tempbuf, "Y Tilt"); break;
                            case kHIDUsage_Dig_Azimuth: sprintf((char *)tempbuf, "Azimuth"); break;
                            case kHIDUsage_Dig_Altitude: sprintf((char *)tempbuf, "Altitude"); break;
                            case kHIDUsage_Dig_Twist: sprintf((char *)tempbuf, "Twist"); break;
                            case kHIDUsage_Dig_TipSwitch: sprintf((char *)tempbuf, "Tip Switch"); break;
                            case kHIDUsage_Dig_SecondaryTipSwitch: sprintf((char *)tempbuf, "Secondary Tip Switch"); break;
                            case kHIDUsage_Dig_BarrelSwitch: sprintf((char *)tempbuf, "Barrel Switch"); break;
                            case kHIDUsage_Dig_Eraser: sprintf((char *)tempbuf, "Eraser"); break;
                            case kHIDUsage_Dig_TabletPick: sprintf((char *)tempbuf, "Tablet Pick"); break;
                            case kHIDUsage_Dig_TouchValid: sprintf((char *)tempbuf, "Touch Valid"); break;
                            case kHIDUsage_Dig_Width: sprintf((char *)tempbuf, "Width"); break;
                            case kHIDUsage_Dig_Height: sprintf((char *)tempbuf, "Height"); break;
                            case kHIDUsage_Dig_GestureCharacterEnable: sprintf((char *)tempbuf, "Gesture Character Enable"); break;
                            case kHIDUsage_Dig_GestureCharacterQuality: sprintf((char *)tempbuf, "Gesture Character Quality"); break;
                            case kHIDUsage_Dig_GestureCharacterDataLength: sprintf((char *)tempbuf, "Gesture Character Data Length"); break;
                            case kHIDUsage_Dig_GestureCharacterData: sprintf((char *)tempbuf, "Gesture Character Data"); break;
                            case kHIDUsage_Dig_GestureCharacterEncoding: sprintf((char *)tempbuf, "Gesture Character Encoding"); break;
                            case kHIDUsage_Dig_GestureCharacterEncodingUTF8: sprintf((char *)tempbuf, "Gesture Character Encoding UTF8"); break;
                            case kHIDUsage_Dig_GestureCharacterEncodingUTF16LE: sprintf((char *)tempbuf, "Gesture Character Encoding UTF16 Little Endian"); break;
                            case kHIDUsage_Dig_GestureCharacterEncodingUTF16BE: sprintf((char *)tempbuf, "Gesture Character Encoding UTF16 Big Endian"); break;
                            case kHIDUsage_Dig_GestureCharacterEncodingUTF32LE: sprintf((char *)tempbuf, "Gesture Character Encoding UTF32 Little Endian"); break;
                            case kHIDUsage_Dig_GestureCharacterEncodingUTF32BE: sprintf((char *)tempbuf, "Gesture Character Encoding UTF32 Big Endian"); break;

                            default: sprintf((char *)tempbuf, "%d (0x%x)", (int)value, (unsigned int)value); break;
                        }
                        strcat((char *)tempbuf, (char *)")");
                    }
                    else if (usagePage == kHIDPage_PID)
                    {
                        strcat((char *)buf, (char *)"(");
                        switch (value)
                        {
                            case 1: sprintf((char *)tempbuf, "Physical Interface Device"); break;
                            case 0x20: sprintf((char *)tempbuf, "Normal"); break;
                            case 0x21: sprintf((char *)tempbuf, "Set Effect Report"); break;
                            case 0x22: sprintf((char *)tempbuf, "Effect Block Index"); break;
                            case 0x23: sprintf((char *)tempbuf, "Parameter Block Offset"); break;
                            case 0x24: sprintf((char *)tempbuf, "ROM Flag"); break;
                            case 0x25: sprintf((char *)tempbuf, "Effect Type"); break;
                            case 0x26: sprintf((char *)tempbuf, "ET Constant Force"); break;
                            case 0x27: sprintf((char *)tempbuf, "ET Ramp"); break;
                            case 0x28: sprintf((char *)tempbuf, "ET Custom Force Data"); break;
                            case 0x30: sprintf((char *)tempbuf, "ET Square"); break;
                            case 0x31: sprintf((char *)tempbuf, "ET Sine"); break;
                            case 0x32: sprintf((char *)tempbuf, "ET Triangle"); break;
                            case 0x33: sprintf((char *)tempbuf, "ET Sawtooth Up"); break;
                            case 0x34: sprintf((char *)tempbuf, "ET Sawtooth Down"); break;
                            case 0x40: sprintf((char *)tempbuf, "ET Spring"); break;
                            case 0x41: sprintf((char *)tempbuf, "ET Damper"); break;
                            case 0x42: sprintf((char *)tempbuf, "ET Inertia"); break;
                            case 0x43: sprintf((char *)tempbuf, "ET Friction"); break;
                            case 0x50: sprintf((char *)tempbuf, "Duration"); break;
                            case 0x51: sprintf((char *)tempbuf, "Sample Period"); break;
                            case 0x52: sprintf((char *)tempbuf, "Gain"); break;
                            case 0x53: sprintf((char *)tempbuf, "Trigger Button"); break;
                            case 0x54: sprintf((char *)tempbuf, "Trigger Repeat Interval"); break;
                            case 0x55: sprintf((char *)tempbuf, "Axes Enable"); break;
                            case 0x56: sprintf((char *)tempbuf, "Direction Enable"); break;
                            case 0x57: sprintf((char *)tempbuf, "Direction"); break;
                            case 0x58: sprintf((char *)tempbuf, "Type Specific Block Offset"); break;
                            case 0x59: sprintf((char *)tempbuf, "Block Type"); break;
                            case 0x5a: sprintf((char *)tempbuf, "Set Envelope Report"); break;
                            case 0x5b: sprintf((char *)tempbuf, "Attack Level"); break;
                            case 0x5c: sprintf((char *)tempbuf, "Attack Time"); break;
                            case 0x5d: sprintf((char *)tempbuf, "Fade Level"); break;
                            case 0x5e: sprintf((char *)tempbuf, "Fade Time"); break;
                            case 0x5f: sprintf((char *)tempbuf, "Set Condition Report"); break;
                            case 0x60: sprintf((char *)tempbuf, "CP Offset"); break;
                            case 0x61: sprintf((char *)tempbuf, "Positive Coefficient"); break;
                            case 0x62: sprintf((char *)tempbuf, "Negative Coefficient"); break;
                            case 0x63: sprintf((char *)tempbuf, "Positive Saturation"); break;
                            case 0x64: sprintf((char *)tempbuf, "Negative Saturation"); break;
                            case 0x65: sprintf((char *)tempbuf, "Dead Band"); break;
                            case 0x66: sprintf((char *)tempbuf, "Download Force Data Report"); break;
                            case 0x67: sprintf((char *)tempbuf, "Isoch Custom Force Enable"); break;
                            case 0x68: sprintf((char *)tempbuf, "Custom Force Data Report"); break;
                            case 0x69: sprintf((char *)tempbuf, "Custom Force Data"); break;
                            case 0x6a: sprintf((char *)tempbuf, "Custom Force Vendor Defined Data"); break;
                            case 0x6b: sprintf((char *)tempbuf, "Set Custom Force Report"); break;
                            case 0x6c: sprintf((char *)tempbuf, "Custom Force Data Offset"); break;
                            case 0x6d: sprintf((char *)tempbuf, "Sample Count"); break;
                            case 0x6e: sprintf((char *)tempbuf, "Set Periodic Report"); break;
                            case 0x6f: sprintf((char *)tempbuf, "Offset"); break;
                            case 0x70: sprintf((char *)tempbuf, "Magnitude"); break;
                            case 0x71: sprintf((char *)tempbuf, "Phase"); break;
                            case 0x72: sprintf((char *)tempbuf, "Period"); break;
                            case 0x73: sprintf((char *)tempbuf, "Set Constant Force Report"); break;
                            case 0x74: sprintf((char *)tempbuf, "Set Constant Force"); break;
                            case 0x75: sprintf((char *)tempbuf, "Ramp Start"); break;
                            case 0x76: sprintf((char *)tempbuf, "Ramp End"); break;
                            case 0x77: sprintf((char *)tempbuf, "Effect Operation Report"); break;
                            case 0x78: sprintf((char *)tempbuf, "Effect Operation"); break;
                            case 0x79: sprintf((char *)tempbuf, "Op Effect Start"); break;
                            case 0x7a: sprintf((char *)tempbuf, "Op Effect Start Solo"); break;
                            case 0x7b: sprintf((char *)tempbuf, "Op Effect Stop"); break;
                            case 0x7c: sprintf((char *)tempbuf, "Loop Count"); break;
                            case 0x7d: sprintf((char *)tempbuf, "Gain Report"); break;
                            case 0x7e: sprintf((char *)tempbuf, "Gain"); break;
                            case 0x7f: sprintf((char *)tempbuf, "PID Pool Report"); break;
                            case 0x80: sprintf((char *)tempbuf, "RAM Pool Size"); break;
                            case 0x81: sprintf((char *)tempbuf, "ROM Pool Size"); break;
                            case 0x82: sprintf((char *)tempbuf, "ROM Effect Block Count"); break;
                            case 0x83: sprintf((char *)tempbuf, "Simultaneous Effects Max"); break;
                            case 0x84: sprintf((char *)tempbuf, "Pool Alignment"); break;
                            case 0x85: sprintf((char *)tempbuf, "PID Pool Move Report"); break;
                            case 0x86: sprintf((char *)tempbuf, "Move Source"); break;
                            case 0x87: sprintf((char *)tempbuf, "Move Destination"); break;
                            case 0x88: sprintf((char *)tempbuf, "Move Length"); break;
                            case 0x89: sprintf((char *)tempbuf, "PID Block Load Report"); break;
                            case 0x8b: sprintf((char *)tempbuf, "Block Load Status"); break;
                            case 0x8c: sprintf((char *)tempbuf, "Block Load Success"); break;
                            case 0x8d: sprintf((char *)tempbuf, "Block Load Full"); break;
                            case 0x8e: sprintf((char *)tempbuf, "Block Load Error"); break;
                            case 0x8f: sprintf((char *)tempbuf, "Block Handle"); break;
                            case 0x90: sprintf((char *)tempbuf, "PID Block Free Report"); break;
                            case 0x91: sprintf((char *)tempbuf, "Type Specific Block Handle"); break;
                            case 0x92: sprintf((char *)tempbuf, "PID State Report"); break;
                            case 0x94: sprintf((char *)tempbuf, "Effect Playing"); break;
                            case 0x95: sprintf((char *)tempbuf, "PID Device Control Report"); break;
                            case 0x96: sprintf((char *)tempbuf, "PID Device Control"); break;
                            case 0x97: sprintf((char *)tempbuf, "DC Enable Actuators"); break;
                            case 0x98: sprintf((char *)tempbuf, "DC Disable Actuators"); break;
                            case 0x99: sprintf((char *)tempbuf, "DC Stoop All Effects"); break;
                            case 0x9a: sprintf((char *)tempbuf, "DC Device Reset"); break;
                            case 0x9b: sprintf((char *)tempbuf, "DC Device Pause"); break;
                            case 0x9c: sprintf((char *)tempbuf, "DC Device Continue"); break;
                            case 0x9f: sprintf((char *)tempbuf, "Device Paused"); break;
                            case 0xa0: sprintf((char *)tempbuf, "Actuators Enabled"); break;
                            case 0xa4: sprintf((char *)tempbuf, "Safety Switch"); break;
                            case 0xa5: sprintf((char *)tempbuf, "Actuator Override Switch"); break;
                            case 0xa6: sprintf((char *)tempbuf, "Actuator Power"); break;
                            case 0xa7: sprintf((char *)tempbuf, "Start Delay"); break;
                            case 0xa8: sprintf((char *)tempbuf, "Parameter Block Size"); break;
                            case 0xa9: sprintf((char *)tempbuf, "Device Managed Pool"); break;
                            case 0xaa: sprintf((char *)tempbuf, "Shared parameter blocks"); break;
                            case 0xab: sprintf((char *)tempbuf, "Create New Effect Report"); break;
                            case 0xac: sprintf((char *)tempbuf, "RAM Pool Available"); break;
                                
                                
                            default: sprintf((char *)tempbuf, "%d (0x%x)", (int)value, (unsigned int)value); break;
                        }
                        strcat((char *)tempbuf, (char *)")");
                    }
                    else
                    {
                        sprintf((char *)tempbuf, "%d (0x%x)", (int)value, (unsigned int)value);
                    }
                    
                    strcat((char *)buf, (char *)tempbuf);
                    tempbuf[0] = 0;    // we don't want to add this again outside the switch
                    tempbufvalue[0] = 0;
                    datahandled = 1;
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
        if (!datahandled )
        {
            if (size) {
                strcat((char *)bufvalue, (char *)"(");
                if (size <= 4)
                {
                    if (itemsigned)
                    {
                        sprintf((char *)tempbufvalue, "%d", (int32_t)svalue);
                    }
                    else
                    {
                        sprintf((char *)tempbufvalue, "%u", (uint32_t)value);
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
            itemsigned=0;
        }
        
        
        // finally add the info
        strcat((char *)bufvalue, " "); // in case bufvalue was empty, add a blank space
        
        
        // this juggling is because the End Collection tags were not nested deep enough in the OutlineView.
        
        /*        if (tag == kReport_TagEndCollection) {
         [self PrintKeyVal:buf val:bufvalue forDevice:deviceNumber atDepth:depth+indentLevel+1 forNode:node];
         }
         else
         [self PrintKeyVal:buf val:bufvalue forDevice:deviceNumber atDepth:depth+indentLevel forNode:node];*/
        
        printf("%s%s\n",buf, bufvalue);
    }
}
