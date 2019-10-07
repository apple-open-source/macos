//
//  IOHIDAssistiveSwitchControlDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDAssistiveSwitchControlDescriptor_h
#define IOHIDAssistiveSwitchControlDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAssistiveSwitchControl \
0x05, 0x09,                  /* (GLOBAL) USAGE_PAGE         0x0009 Button Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00090001: Page=Button Page, Usage=Button 1 Primary/trigger, Type=MULTI) <-- Warning: USAGE type should be CA (Application) */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x00090001: Page=Button Page, Usage=Button 1 Primary/trigger, Type=MULTI) <-- Warning: USAGE type should be CP (Physical)   */\
0x19, 0x01,                  /*     (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x29, 0x10,                  /*     (LOCAL)  USAGE_MAXIMUM      0x00090010 Button 16 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x95, 0x10,                  /*     (GLOBAL) REPORT_COUNT       0x10 (16) Number of fields       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (16 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x0A,                  /*     (LOCAL)  USAGE              0x0001000A Assistive Control (CA=Application Collection)      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Button Page HIDAssistiveSwitchControlInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   16
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:16
    // Locals:  USAG:0 UMIN:00090001 UMAX:00090010 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001 00090002 00090003 00090004 00090005 00090006 00090007 00090008 00090009 0009000A 0009000B 0009000C 0009000D 0009000E 0009000F 00090010
    // Coll:    Button1Primary/trigger Button1Primary/trigger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: Button1Primary/trigger Button1Primary/trigger
    uint8_t  BTN_Button1PrimarytriggerButton1 : 1;     // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton2 : 1;     // Usage 0x00090002: Button 2 Secondary, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton3 : 1;     // Usage 0x00090003: Button 3 Tertiary, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton4 : 1;     // Usage 0x00090004: Button 4, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton5 : 1;     // Usage 0x00090005: Button 5, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton6 : 1;     // Usage 0x00090006: Button 6, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton7 : 1;     // Usage 0x00090007: Button 7, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton8 : 1;     // Usage 0x00090008: Button 8, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton9 : 1;     // Usage 0x00090009: Button 9, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton10 : 1;    // Usage 0x0009000A: Button 10, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton11 : 1;    // Usage 0x0009000B: Button 11, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton12 : 1;    // Usage 0x0009000C: Button 12, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton13 : 1;    // Usage 0x0009000D: Button 13, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton14 : 1;    // Usage 0x0009000E: Button 14, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton15 : 1;    // Usage 0x0009000F: Button 15, Value = 0 to 1
    uint8_t  BTN_Button1PrimarytriggerButton16 : 1;    // Usage 0x00090010: Button 16, Value = 0 to 1
} HIDAssistiveSwitchControlInputReport;

#endif /* IOHIDAssistiveSwitchControlDescriptor_h */
