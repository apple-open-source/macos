//
//  IOHIDReportDescriptorParser.c
//  IOHIDFamily
//
//  Created by Rob Yepez on 2/23/13.
//
//

#include <string.h>
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

// Usage pages from HID Usage Tables spec 1.0
enum
{
    kUsage_PageGenericDesktop            = 0x01,
    kUsage_PageSimulationControls        = 0x02,
    kUsage_PageVRControls                = 0x03,
    kUsage_PageSportControls            = 0x04,
    kUsage_PageGameControls                = 0x05,
    kUsage_PageKeyboard                    = 0x07,
    kUsage_PageLED                        = 0x08,
    kUsage_PageButton                    = 0x09,
    kUsage_PageOrdinal                    = 0x0A,
    kUsage_PageTelephonyDevice            = 0x0B,
    kUsage_PageConsumer                    = 0x0C,
    kUsage_PageDigitizers                = 0x0D,
    kUsage_PagePID                = 0x0F,
    kUsage_PageUnicode                    = 0x10,
    kUsage_PageAlphanumericDisplay        = 0x14,
    kUsage_PageMonitor                    = 0x80,
    kUsage_PageMonitorEnumeratedValues    = 0x81,
    kUsage_PageMonitorVirtualControl     = 0x82,
    kUsage_PageMonitorReserved            = 0x83,
    kUsage_PagePowerDevice                = 0x84,
    kUsage_PageBatterySystem            = 0x85,
    kUsage_PowerClassReserved            = 0x86,
    kUsage_PowerClassReserved2            = 0x87
};

// Usage constants for Generic Desktop page (01) from HID Usage Tables spec 1.0
enum
{
    kUsage_01_Pointer        = 0x01,
    kUsage_01_Mouse            = 0x02,
    kUsage_01_Joystick        = 0x04,
    kUsage_01_GamePad        = 0x05,
    kUsage_01_Keyboard        = 0x06,
    kUsage_01_Keypad        = 0x07,
    
    kUsage_01_X                = 0x30,
    kUsage_01_Y                = 0x31,
    kUsage_01_Z                = 0x32,
    kUsage_01_Rx            = 0x33,
    kUsage_01_Ry            = 0x34,
    kUsage_01_Rz            = 0x35,
    kUsage_01_Slider        = 0x36,
    kUsage_01_Dial            = 0x37,
    kUsage_01_Wheel            = 0x38,
    kUsage_01_HatSwitch        = 0x39,
    kUsage_01_CountedBuffer    = 0x3A,
    kUsage_01_ByteCount        = 0x3B,
    kUsage_01_MotionWakeup    = 0x3C,
    
    kUsage_01_Vx            = 0x40,
    kUsage_01_Vy            = 0x41,
    kUsage_01_Vz            = 0x42,
    kUsage_01_Vbrx            = 0x43,
    kUsage_01_Vbry            = 0x44,
    kUsage_01_Vbrz            = 0x45,
    kUsage_01_Vno            = 0x46,
    
    kUsage_01_SystemControl        = 0x80,
    kUsage_01_SystemPowerDown     = 0x81,
    kUsage_01_SystemSleep         = 0x82,
    kUsage_01_SystemWakeup        = 0x83,
    kUsage_01_SystemContextMenu = 0x84,
    kUsage_01_SystemMainMenu    = 0x85,
    kUsage_01_SystemAppMenu        = 0x86,
    kUsage_01_SystemMenuHelp    = 0x87,
    kUsage_01_SystemMenuExit    = 0x88,
    kUsage_01_SystemMenuSelect    = 0x89,
    kUsage_01_SystemMenuRight    = 0x8A,
    kUsage_01_SystemMenuLeft    = 0x8B,
    kUsage_01_SystemMenuUp        = 0x8C,
    kUsage_01_SystemMenuDown    = 0x8D
};

void PrintHIDReport(uint8_t *reportDesc, uint32_t length)
{
    uint8_t *               end = reportDesc + length;
    uint8_t                 size, type, tag;
    uint32_t                usagePage = 0;
    uint32_t                value=0;
    int32_t                 svalue=0;
    static unsigned char    buf[350], tempbuf[350], bufvalue[350], tempbufvalue[350];
    int                     i, indentLevel;
    int                     datahandled=0;
    int                     usagesigned=0;
    
    
    indentLevel = 1;
    
    while (reportDesc < end)
    {
        size = UnpackReportSize(*reportDesc);
        if (size == 3) size = 4;    // 0 == 0 bytes, 1 == 1 bytes, 2 == 2 bytes, but 3 == 4 bytes
        
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
                case 1: svalue = (int8_t) value; break;
                case 2: svalue = (int16_t) value; break;
                    
                    // if the top bit is set, then sign extend it and fall thru to 32bit case
                case 3: if (value & 0x00800000) value |= 0xFF000000; // no break
                case 4: svalue = (int32_t) value; break;
            }
        }
        
        // indent this line
        buf[0] = 0;
        bufvalue[0] = 0;
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
                    
                    strcat((char *)bufvalue, (value & kIO_Array_or_Variable) ? "Variable, ": "Array, ");
                    
                    strcat((char *)bufvalue, (value & kIO_Absolute_or_Relative) ? "Relative" : "Absolute");
                    
                    if (((tag == kReport_TagInput) && (value & kIO_Array_or_Variable)) || tag != kReport_TagInput)
                    {    // these are only valid for variable inputs, and feature/output tags
                        strcat((char *)bufvalue, (value & kIO_NoWrap_or_Wrap) ? ", Wrap, " : ", No Wrap, ");
                        
                        strcat((char *)bufvalue, (value & kIO_Linear_or_NonLinear) ? "Nonlinear, " : "Linear, ");
                        
                        strcat((char *)bufvalue, (value & kIO_PreferredState_or_NoPreferred) ? "No Preferred, " : "Preferred State, ");
                        
                        strcat((char *)bufvalue, (value & kIO_NoNullPosition_or_NullState) ? "Null State, " : "No Null Position, ");
                        
                        if (tag != kReport_TagInput)
                            strcat((char *)bufvalue, (value & kIO_NonVolatile_or_Volatile) ? "Volatile, " : "Nonvolatile, ");
                        
                        strcat((char *)bufvalue, (value & kIO_BitField_or_BufferedBytes) ? "Buffered bytes" : "Bitfield");
                    }
                    
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
                    
                    usagesigned = 1;
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
                        usagesigned = 0;
                        break;
                        
                    case kUsage_PageLED: sprintf((char *)tempbufvalue, "LED"); break;
                    case kUsage_PageButton: sprintf((char *)tempbufvalue, "Button"); break;
                    case kUsage_PageOrdinal: sprintf((char *)tempbufvalue, "Ordinal"); break;
                    case kUsage_PageTelephonyDevice: sprintf((char *)tempbufvalue, "Telephony Device"); break;
                    case kUsage_PageConsumer: sprintf((char *)tempbufvalue, "Consumer"); break;
                    case kUsage_PageDigitizers: sprintf((char *)tempbufvalue, "Digitizer"); break;
                    case kUsage_PagePID: sprintf((char *)tempbufvalue, "PID"); break;
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
                    else if (usagePage == kUsage_PagePID)
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
        if (!datahandled && size)
        {
            strcat((char *)bufvalue, (char *)"(");
            if (size <= 4)
            {
                if (usagesigned)
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
