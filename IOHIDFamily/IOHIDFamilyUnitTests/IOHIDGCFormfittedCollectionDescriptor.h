//
//  IOHIDGCFormFittedCollectionDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDGCFormFittedCollectionDescriptor_h
#define IOHIDGCFormFittedCollectionDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDGCFormFittedCollection \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x05,                  /* (LOCAL)  USAGE              0x00010005 Game Pad (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010005: Page=Generic Desktop Page, Usage=Game Pad, Type=CA) */\
0x09, 0x05,                  /*   (LOCAL)  USAGE              0x00010005 Game Pad (CA=Application Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x00010005: Page=Generic Desktop Page, Usage=Game Pad, Type=CA) <-- Warning: USAGE type should be CL (Logical)   */\
0x15, 0x81,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x81 (-127)       */\
0x25, 0x7F,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x7F (127)       */\
0x35, 0x81,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x81 (-127)       */\
0x45, 0x7F,                  /*     (GLOBAL) PHYSICAL_MAXIMUM   0x7F (127)       */\
0x09, 0x01,                  /*     (LOCAL)  USAGE              0x00010001 Pointer (CP=Physical Collection)      */\
0xA1, 0x00,                  /*     (MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=CP)     */\
0x75, 0x08,                  /*       (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field         */\
0x95, 0x04,                  /*       (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields         */\
0x09, 0x30,                  /*       (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)        */\
0x09, 0x31,                  /*       (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)        */\
0x09, 0x32,                  /*       (LOCAL)  USAGE              0x00010032 Z (DV=Dynamic Value)        */\
0x09, 0x35,                  /*       (LOCAL)  USAGE              0x00010035 Rz (DV=Dynamic Value)        */\
0x81, 0x02,                  /*       (MAIN)   INPUT              0x00000002 (4 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap        */\
0xC0,                        /*     (MAIN)   END_COLLECTION     Physical     */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0)  <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x00,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)       */\
0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0)  <-- Info: Consider replacing 35 00 with 34     */\
0x46, 0xFF, 0x00,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x00FF (255)       */\
0x05, 0x09,                  /*     (GLOBAL) USAGE_PAGE         0x0009 Button Page      */\
0x09, 0x07,                  /*     (LOCAL)  USAGE              0x00090007 Button 7 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x09, 0x08,                  /*     (LOCAL)  USAGE              0x00090008 Button 8 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x04,                  /*     (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields       */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x45, 0x01,                  /*     (GLOBAL) PHYSICAL_MAXIMUM   0x01 (1)       */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x90,                  /*     (LOCAL)  USAGE              0x00010090 D-pad Up (OOC=On/Off Control)      */\
0x09, 0x92,                  /*     (LOCAL)  USAGE              0x00010092 D-pad Right (OOC=On/Off Control)      */\
0x09, 0x91,                  /*     (LOCAL)  USAGE              0x00010091 D-pad Down (OOC=On/Off Control)      */\
0x09, 0x93,                  /*     (LOCAL)  USAGE              0x00010093 D-pad Left (OOC=On/Off Control)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (4 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x06,                  /*     (GLOBAL) REPORT_COUNT       0x06 (6) Number of fields       */\
0x05, 0x09,                  /*     (GLOBAL) USAGE_PAGE         0x0009 Button Page      */\
0x19, 0x01,                  /*     (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x29, 0x06,                  /*     (LOCAL)  USAGE_MAXIMUM      0x00090006 Button 6 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (6 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x09, 0x09,                  /*     (LOCAL)  USAGE              0x00090009 Button 9 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x09, 0x0A,                  /*     (LOCAL)  USAGE              0x0009000A Button 10 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x05, 0x0C,                  /*     (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page      */\
0x0A, 0x23, 0x02,            /*     (LOCAL)  USAGE              0x000C0223 AC Home (Sel=Selector)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x03,                  /*     (GLOBAL) REPORT_COUNT       0x03 (3) Number of fields       */\
0x81, 0x03,                  /*     (MAIN)   INPUT              0x00000003 (3 fields x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x04,                  /*     (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields       */\
0x05, 0x08,                  /*     (GLOBAL) USAGE_PAGE         0x0008 LED Indicator Page      */\
0x1A, 0x00, 0xFF,            /*     (LOCAL)  USAGE_MINIMUM      0x0008FF00       */\
0x2A, 0x03, 0xFF,            /*     (LOCAL)  USAGE_MAXIMUM      0x0008FF03       */\
0x91, 0x02,                  /*     (MAIN)   OUTPUT             0x00000002 (4 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x91, 0x03,                  /*     (MAIN)   OUTPUT             0x00000003 (4 fields x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x05, 0x05,                  /*     (GLOBAL) USAGE_PAGE         0x0005 Game Controls Page      */\
0x09, 0x3A,                  /*     (LOCAL)  USAGE              0x0005003A Form-fitting Gamepad (SF=Static Flag)      */\
0xA1, 0x02,                  /*     (MAIN)   COLLECTION         0x02 Logical (Usage=0x0005003A: Page=Game Controls Page, Usage=Form-fitting Gamepad, Type=SF) <-- Warning: USAGE type should be CL (Logical)     */\
0xC0,                        /*     (MAIN)   END_COLLECTION     Logical     */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Generic Desktop Page HIDGCFormFittedCollectionInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   8
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:-127 LMAX:127 PMIN:-127 PMAX:127 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:4
    // Locals:  USAG:00010035 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030 00010031 00010032 00010035
    // Coll:    GamePad GamePad Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    // Collection: GamePad GamePad Pointer
    int8_t   GD_GamePadPointerX;                       // Usage 0x00010030: X, Value = -127 to 127, Physical = ((Value + 127) - 127)
    int8_t   GD_GamePadPointerY;                       // Usage 0x00010031: Y, Value = -127 to 127, Physical = ((Value + 127) - 127)
    int8_t   GD_GamePadPointerZ;                       // Usage 0x00010032: Z, Value = -127 to 127, Physical = ((Value + 127) - 127)
    int8_t   GD_GamePadPointerRz;                      // Usage 0x00010035: Rz, Value = -127 to 127, Physical = ((Value + 127) - 127)
    
    // Field:   2
    // Width:   8
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:255 PMIN:0 PMAX:255 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:2
    // Locals:  USAG:00090008 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090007 00090008
    // Coll:    GamePad GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: GamePad GamePad
    uint8_t  BTN_GamePadButton7;                       // Usage 0x00090007: Button 7, Value = 0 to 255, Physical = Value
    uint8_t  BTN_GamePadButton8;                       // Usage 0x00090008: Button 8, Value = 0 to 255, Physical = Value
    
    // Field:   3
    // Width:   1
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:4
    // Locals:  USAG:00010093 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010090 00010092 00010091 00010093
    // Coll:    GamePad GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint8_t  GD_GamePadDPadUp : 1;                     // Usage 0x00010090: D-pad Up, Value = 0 to 1, Physical = Value
    uint8_t  GD_GamePadDPadRight : 1;                  // Usage 0x00010092: D-pad Right, Value = 0 to 1, Physical = Value
    uint8_t  GD_GamePadDPadDown : 1;                   // Usage 0x00010091: D-pad Down, Value = 0 to 1, Physical = Value
    uint8_t  GD_GamePadDPadLeft : 1;                   // Usage 0x00010093: D-pad Left, Value = 0 to 1, Physical = Value
    
    // Field:   4
    // Width:   1
    // Count:   6
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:6
    // Locals:  USAG:0 UMIN:00090001 UMAX:00090006 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001 00090002 00090003 00090004 00090005 00090006
    // Coll:    GamePad GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    uint8_t  BTN_GamePadButton1 : 1;                   // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1, Physical = Value
    uint8_t  BTN_GamePadButton2 : 1;                   // Usage 0x00090002: Button 2 Secondary, Value = 0 to 1, Physical = Value
    uint8_t  BTN_GamePadButton3 : 1;                   // Usage 0x00090003: Button 3 Tertiary, Value = 0 to 1, Physical = Value
    uint8_t  BTN_GamePadButton4 : 1;                   // Usage 0x00090004: Button 4, Value = 0 to 1, Physical = Value
    uint8_t  BTN_GamePadButton5 : 1;                   // Usage 0x00090005: Button 5, Value = 0 to 1, Physical = Value
    uint8_t  BTN_GamePadButton6 : 1;                   // Usage 0x00090006: Button 6, Value = 0 to 1, Physical = Value
    
    // Field:   5
    // Width:   1
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:2
    // Locals:  USAG:0009000A UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090009 0009000A
    // Coll:    GamePad GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    uint8_t  BTN_GamePadButton9 : 1;                   // Usage 0x00090009: Button 9, Value = 0 to 1, Physical = Value
    uint8_t  BTN_GamePadButton10 : 1;                  // Usage 0x0009000A: Button 10, Value = 0 to 1, Physical = Value
    
    // Field:   6
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000C0223 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C0223
    // Coll:    GamePad GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    uint8_t  CD_GamePadAcHome : 1;                     // Usage 0x000C0223: AC Home, Value = 0 to 1, Physical = Value
    
    // Field:   7
    // Width:   1
    // Count:   3
    // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:3
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    GamePad GamePad
    // Access:  Read/Only
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
} HIDGCFormFittedCollectionInputReport;


//--------------------------------------------------------------------------------
// LED Indicator Page HIDGCFormFittedCollectionOutputReport (Device <-- Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0008 LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:4
    // Locals:  USAG:0 UMIN:0008FF00 UMAX:0008FF03 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  0008FF00 0008FF01 0008FF02 0008FF03
    // Coll:    GamePad GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0008: LED Indicator Page
    // Collection: GamePad GamePad
    uint8_t  LED_GamePadFF00 : 1;                      // Usage 0x0008FF00: , Value = 0 to 1, Physical = Value
    uint8_t  LED_GamePadFF01 : 1;                      // Usage 0x0008FF01: , Value = 0 to 1, Physical = Value
    uint8_t  LED_GamePadFF02 : 1;                      // Usage 0x0008FF02: , Value = 0 to 1, Physical = Value
    uint8_t  LED_GamePadFF03 : 1;                      // Usage 0x0008FF03: , Value = 0 to 1, Physical = Value
    
    // Field:   2
    // Width:   1
    // Count:   4
    // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0008 LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:4
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    GamePad GamePad
    // Access:  Read/Only
    // Type:    Variable
    // Page 0x0008: LED Indicator Page
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
} HIDGCFormFittedCollectionOutputReport;

#endif /* IOHIDGCFormFittedCollectionDescriptor_h */
