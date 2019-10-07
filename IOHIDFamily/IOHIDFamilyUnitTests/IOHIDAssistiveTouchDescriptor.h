//
//  IOHIDAssistiveTouchDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDAssistiveTouchDescriptor_h
#define IOHIDAssistiveTouchDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAssistiveTouch \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x02,                  /* (LOCAL)  USAGE              0x00010002 Mouse (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010002: Page=Generic Desktop Page, Usage=Mouse, Type=CA) */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0x00010001 Pointer (CP=Physical Collection)    */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=CP)   */\
0x05, 0x09,                  /*     (GLOBAL) USAGE_PAGE         0x0009 Button Page      */\
0x19, 0x01,                  /*     (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x29, 0x03,                  /*     (LOCAL)  USAGE_MAXIMUM      0x00090003 Button 3 Tertiary (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x95, 0x03,                  /*     (GLOBAL) REPORT_COUNT       0x03 (3) Number of fields       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (3 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x75, 0x05,                  /*     (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field       */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x09, 0x0A,                  /*     (LOCAL)  USAGE              0x0001000A Assistive Control (CA=Application Collection)      */\
0x15, 0x81,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x81 (-127)       */\
0x25, 0x7F,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x7F (127)       */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x81, 0x06,                  /*     (MAIN)   INPUT              0x00000006 (2 fields x 8 bits) 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Button Page HIDAssistiveTouchInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   3
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:3
    // Locals:  USAG:0 UMIN:00090001 UMAX:00090003 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001 00090002 00090003
    // Coll:    Mouse Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: Mouse Pointer
    uint8_t  BTN_MousePointerButton1 : 1;              // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
    uint8_t  BTN_MousePointerButton2 : 1;              // Usage 0x00090002: Button 2 Secondary, Value = 0 to 1
    uint8_t  BTN_MousePointerButton3 : 1;              // Usage 0x00090003: Button 3 Tertiary, Value = 0 to 1
    
    // Field:   2
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Mouse Pointer
    // Access:  Read/Only
    // Type:    Array
    // Page 0x0009: Button Page
    uint8_t  : 5;                                      // Pad
    
    // Field:   3
    // Width:   8
    // Count:   2
    // Flags:   00000006: 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:-127 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:2
    // Locals:  USAG:0001000A UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030 00010031 0001000A
    // Coll:    Mouse Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    int8_t   GD_MousePointerX;                         // Usage 0x00010030: X, Value = -127 to 127
    int8_t   GD_MousePointerY;                         // Usage 0x00010031: Y, Value = -127 to 127
    // Usage 0x0001000A Assistive Control (CA=Application Collection) Value = -127 to 127 <-- Ignored: REPORT_COUNT (2) is too small
} HIDAssistiveTouchInputReport;

#endif /* IOHIDAssistiveTouchDescriptor_h */
