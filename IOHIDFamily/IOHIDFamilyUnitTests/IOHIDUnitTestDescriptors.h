//
//  IOHIDUnitTestDescriptors.h
//  IOHIDFamily
//
//  Created by yg on 1/5/17.
//
//

#ifndef IOHIDUnitTestDescriptors_h
#define IOHIDUnitTestDescriptors_h



//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDKeyboardDescriptor \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x06,                  /* (LOCAL)  USAGE              0x00010006 Keyboard (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010006: Page=Generic Desktop Page, Usage=Keyboard, Type=CA) */\
0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */\
0x19, 0xE0,                  /*   (LOCAL)  USAGE_MINIMUM      0x000700E0 Keyboard Left Control (DV=Dynamic Value)    */\
0x29, 0xE7,                  /*   (LOCAL)  USAGE_MAXIMUM      0x000700E7 Keyboard Right GUI (DV=Dynamic Value)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x08,                  /*   (GLOBAL) REPORT_COUNT       0x08 (8) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (8 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 8 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x05, 0x08,                  /*   (GLOBAL) USAGE_PAGE         0x0008 LED Indicator Page    */\
0x19, 0x01,                  /*   (LOCAL)  USAGE_MINIMUM      0x00080001 Num Lock (OOC=On/Off Control)    */\
0x29, 0x05,                  /*   (LOCAL)  USAGE_MAXIMUM      0x00080005 Kana (OOC=On/Off Control)    */\
0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x91, 0x02,                  /*   (MAIN)   OUTPUT             0x00000002 (5 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x03,                  /*   (GLOBAL) REPORT_SIZE        0x03 (3) Number of bits per field     */\
0x91, 0x01,                  /*   (MAIN)   OUTPUT             0x00000001 (1 field x 3 bits) 1=Constant 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */\
0x19, 0x00,                  /*   (LOCAL)  USAGE_MINIMUM      0x00070000 Keyboard No event indicated (Sel=Selector)    */\
0x2A, 0xFF, 0x00,            /*   (LOCAL)  USAGE_MAXIMUM      0x000700FF     */\
0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x26, 0xFF, 0x00,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)     */\
0x81, 0x00,                  /*   (MAIN)   INPUT              0x00000000 (5 fields x 8 bits) 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x05, 0xFF,                  /*   (GLOBAL) USAGE_PAGE         0x00FF Reserved    */\
0x09, 0x03,                  /*   (LOCAL)  USAGE              0x00FF0003     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Keyboard/Keypad Page HIDKeyboardDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   8
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:8
    // Locals:  USAG:0 UMIN:000700E0 UMAX:000700E7 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000700E0 000700E1 000700E2 000700E3 000700E4 000700E5 000700E6 000700E7
    // Coll:    Keyboard
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0007: Keyboard/Keypad Page
    // Collection: Keyboard
    uint8_t  KB_KeyboardKeyboardLeftControl : 1;       // Usage 0x000700E0: Keyboard Left Control, Value = 0 to 1
    uint8_t  KB_KeyboardKeyboardLeftShift : 1;         // Usage 0x000700E1: Keyboard Left Shift, Value = 0 to 1
    uint8_t  KB_KeyboardKeyboardLeftAlt : 1;           // Usage 0x000700E2: Keyboard Left Alt, Value = 0 to 1
    uint8_t  KB_KeyboardKeyboardLeftGui : 1;           // Usage 0x000700E3: Keyboard Left GUI, Value = 0 to 1
    uint8_t  KB_KeyboardKeyboardRightControl : 1;      // Usage 0x000700E4: Keyboard Right Control, Value = 0 to 1
    uint8_t  KB_KeyboardKeyboardRightShift : 1;        // Usage 0x000700E5: Keyboard Right Shift, Value = 0 to 1
    uint8_t  KB_KeyboardKeyboardRightAlt : 1;          // Usage 0x000700E6: Keyboard Right Alt, Value = 0 to 1
    uint8_t  KB_KeyboardKeyboardRightGui : 1;          // Usage 0x000700E7: Keyboard Right GUI, Value = 0 to 1
    
    // Field:   2
    // Width:   8
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Keyboard
    // Access:  Read/Only
    // Type:    Array
    // Page 0x0007: Keyboard/Keypad Page
    uint8_t  pad_2;                                    // Pad
    
    // Field:   3
    // Width:   8
    // Count:   5
    // Flags:   00000000: 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:0007 LMIN:0 LMAX:255 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:5
    // Locals:  USAG:0 UMIN:00070000 UMAX:000700FF DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00070000 00070001 00070002 00070003 00070004 00070005 00070006 00070007 00070008 00070009 0007000A 0007000B 0007000C 0007000D 0007000E 0007000F 00070010 00070011 00070012 00070013 00070014 00070015 00070016 00070017 00070018 00070019 0007001A 0007001B 0007001C 0007001D 0007001E 0007001F 00070020 00070021 00070022 00070023 00070024 00070025 00070026 00070027 00070028 00070029 0007002A 0007002B 0007002C 0007002D 0007002E 0007002F 00070030 00070031 00070032 00070033 00070034 00070035 00070036 00070037 00070038 00070039 0007003A 0007003B 0007003C 0007003D 0007003E 0007003F 00070040 00070041 00070042 00070043 00070044 00070045 00070046 00070047 00070048 00070049 0007004A 0007004B 0007004C 0007004D 0007004E 0007004F 00070050 00070051 00070052 00070053 00070054 00070055 00070056 00070057 00070058 00070059 0007005A 0007005B 0007005C 0007005D 0007005E 0007005F 00070060 00070061 00070062 00070063 00070064 00070065 00070066 00070067 00070068 00070069 0007006A 0007006B 0007006C 0007006D 0007006E 0007006F 00070070 00070071 00070072 00070073 00070074 00070075 00070076 00070077 00070078 00070079 0007007A 0007007B 0007007C 0007007D 0007007E 0007007F 00070080 00070081 00070082 00070083 00070084 00070085 00070086 00070087 00070088 00070089 0007008A 0007008B 0007008C 0007008D 0007008E 0007008F 00070090 00070091 00070092 00070093 00070094 00070095 00070096 00070097 00070098 00070099 0007009A 0007009B 0007009C 0007009D 0007009E 0007009F 000700A0 000700A1 000700A2 000700A3 000700A4 000700A5 000700A6 000700A7 000700A8 000700A9 000700AA 000700AB 000700AC 000700AD 000700AE 000700AF 000700B0 000700B1 000700B2 000700B3 000700B4 000700B5 000700B6 000700B7 000700B8 000700B9 000700BA 000700BB 000700BC 000700BD 000700BE 000700BF 000700C0 000700C1 000700C2 000700C3 000700C4 000700C5 000700C6 000700C7 000700C8 000700C9 000700CA 000700CB 000700CC 000700CD 000700CE 000700CF 000700D0 000700D1 000700D2 000700D3 000700D4 000700D5 000700D6 000700D7 000700D8 000700D9 000700DA 000700DB 000700DC 000700DD 000700DE 000700DF 000700E0 000700E1 000700E2 000700E3 000700E4 000700E5 000700E6 000700E7 000700E8 000700E9 000700EA 000700EB 000700EC 000700ED 000700EE 000700EF 000700F0 000700F1 000700F2 000700F3 000700F4 000700F5 000700F6 000700F7 000700F8 000700F9 000700FA 000700FB 000700FC 000700FD 000700FE 000700FF
    // Coll:    Keyboard
    // Access:  Read/Write
    // Type:    Array
    // Page 0x0007: Keyboard/Keypad Page
    uint8_t  KB_Keyboard[5];                           // Value = 0 to 255
    
    // Field:   4
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:00FF LMIN:0 LMAX:255 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:00FF0003 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00FF0003
    // Coll:    Keyboard
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x00FF: Reserved
    uint8_t  RES_Keyboard0003;                         // Usage 0x00FF0003: , Value = 0 to 255
} HIDKeyboardDescriptorInputReport;


//--------------------------------------------------------------------------------
// LED Indicator Page HIDKeyboardDescriptorOutputReport (Device <-- Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   5
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0008 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:5
    // Locals:  USAG:0 UMIN:00080001 UMAX:00080005 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00080001 00080002 00080003 00080004 00080005
    // Coll:    Keyboard
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0008: LED Indicator Page
    // Collection: Keyboard
    uint8_t  LED_KeyboardNumLock : 1;                  // Usage 0x00080001: Num Lock, Value = 0 to 1
    uint8_t  LED_KeyboardCapsLock : 1;                 // Usage 0x00080002: Caps Lock, Value = 0 to 1
    uint8_t  LED_KeyboardScrollLock : 1;               // Usage 0x00080003: Scroll Lock, Value = 0 to 1
    uint8_t  LED_KeyboardCompose : 1;                  // Usage 0x00080004: Compose, Value = 0 to 1
    uint8_t  LED_KeyboardKana : 1;                     // Usage 0x00080005: Kana, Value = 0 to 1
    
    // Field:   2
    // Width:   3
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0008 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:3 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Keyboard
    // Access:  Read/Only
    // Type:    Array
    // Page 0x0008: LED Indicator Page
    uint8_t  : 3;                                      // Pad
} HIDKeyboardDescriptorOutputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDGameControllerDescriptor \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x05,                  /* (LOCAL)  USAGE              0x00010005 Game Pad (CA=Application Collection)  */\
0x09, 0x3A,                  /* (LOCAL)  USAGE              0x0001003A Counted Buffer (CL=Logical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x0001003A: Page=Generic Desktop Page, Usage=Counted Buffer, Type=CL) <-- Warning: USAGE type should be CA (Application) */\
0x09, 0x05,                  /*   (LOCAL)  USAGE              0x00010005 Game Pad (CA=Application Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x00010005: Page=Generic Desktop Page, Usage=Game Pad, Type=CA) <-- Warning: USAGE type should be CL (Logical)   */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x04,                  /*     (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields       */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x00,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)       */\
0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34     */\
0x46, 0xFF, 0x00,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x00FF (255)       */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page <-- Redundant: USAGE_PAGE is already 0x0001     */\
0x09, 0x90,                  /*     (LOCAL)  USAGE              0x00010090 D-pad Up (OOC=On/Off Control)      */\
0x09, 0x92,                  /*     (LOCAL)  USAGE              0x00010092 D-pad Right (OOC=On/Off Control)      */\
0x09, 0x91,                  /*     (LOCAL)  USAGE              0x00010091 D-pad Down (OOC=On/Off Control)      */\
0x09, 0x93,                  /*     (LOCAL)  USAGE              0x00010093 D-pad Left (OOC=On/Off Control)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (4 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x08,                  /*     (GLOBAL) REPORT_COUNT       0x08 (8) Number of fields       */\
0x05, 0x09,                  /*     (GLOBAL) USAGE_PAGE         0x0009 Button Page      */\
0x19, 0x01,                  /*     (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x29, 0x08,                  /*     (LOCAL)  USAGE_MAXIMUM      0x00090008 Button 8 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (8 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34     */\
0x45, 0x01,                  /*     (GLOBAL) PHYSICAL_MAXIMUM   0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x05, 0x0C,                  /*     (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page      */\
0x0A, 0x23, 0x02,            /*     (LOCAL)  USAGE              0x000C0223 AC Home (Sel=Selector)      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x07,                  /*     (GLOBAL) REPORT_COUNT       0x07 (7) Number of fields       */\
0x81, 0x03,                  /*     (MAIN)   INPUT              0x00000003 (7 fields x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x15, 0x81,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x81 (-127)       */\
0x25, 0x7F,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x7F (127)       */\
0x35, 0x81,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x81 (-127)       */\
0x45, 0x7F,                  /*     (GLOBAL) PHYSICAL_MAXIMUM   0x7F (127)       */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
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
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Generic Desktop Page HIDGameControllerDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   8
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:255 PMIN:0 PMAX:255 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:4
    // Locals:  USAG:00010093 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010090 00010092 00010091 00010093
    // Coll:    CountedBuffer GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    // Collection: CountedBuffer GamePad
    uint8_t  GD_CountedBufferGamePadDPadUp;            // Usage 0x00010090: D-pad Up, Value = 0 to 255, Physical = Value
    uint8_t  GD_CountedBufferGamePadDPadRight;         // Usage 0x00010092: D-pad Right, Value = 0 to 255, Physical = Value
    uint8_t  GD_CountedBufferGamePadDPadDown;          // Usage 0x00010091: D-pad Down, Value = 0 to 255, Physical = Value
    uint8_t  GD_CountedBufferGamePadDPadLeft;          // Usage 0x00010093: D-pad Left, Value = 0 to 255, Physical = Value
    
    // Field:   2
    // Width:   8
    // Count:   8
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:255 PMIN:0 PMAX:255 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:8
    // Locals:  USAG:0 UMIN:00090001 UMAX:00090008 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001 00090002 00090003 00090004 00090005 00090006 00090007 00090008
    // Coll:    CountedBuffer GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    uint8_t  BTN_CountedBufferGamePadButton1;          // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 255, Physical = Value
    uint8_t  BTN_CountedBufferGamePadButton2;          // Usage 0x00090002: Button 2 Secondary, Value = 0 to 255, Physical = Value
    uint8_t  BTN_CountedBufferGamePadButton3;          // Usage 0x00090003: Button 3 Tertiary, Value = 0 to 255, Physical = Value
    uint8_t  BTN_CountedBufferGamePadButton4;          // Usage 0x00090004: Button 4, Value = 0 to 255, Physical = Value
    uint8_t  BTN_CountedBufferGamePadButton5;          // Usage 0x00090005: Button 5, Value = 0 to 255, Physical = Value
    uint8_t  BTN_CountedBufferGamePadButton6;          // Usage 0x00090006: Button 6, Value = 0 to 255, Physical = Value
    uint8_t  BTN_CountedBufferGamePadButton7;          // Usage 0x00090007: Button 7, Value = 0 to 255, Physical = Value
    uint8_t  BTN_CountedBufferGamePadButton8;          // Usage 0x00090008: Button 8, Value = 0 to 255, Physical = Value
    
    // Field:   3
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000C0223 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C0223
    // Coll:    CountedBuffer GamePad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    uint8_t  CD_CountedBufferGamePadAcHome : 1;        // Usage 0x000C0223: AC Home, Value = 0 to 1, Physical = Value
    
    // Field:   4
    // Width:   1
    // Count:   7
    // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:7
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    CountedBuffer GamePad
    // Access:  Read/Only
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    
    // Field:   5
    // Width:   8
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:-127 LMAX:127 PMIN:-127 PMAX:127 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:4
    // Locals:  USAG:00010035 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030 00010031 00010032 00010035
    // Coll:    CountedBuffer GamePad Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    // Collection: CountedBuffer GamePad Pointer
    int8_t   GD_CountedBufferGamePadPointerX;          // Usage 0x00010030: X, Value = -127 to 127, Physical = ((Value + 127) - 127)
    int8_t   GD_CountedBufferGamePadPointerY;          // Usage 0x00010031: Y, Value = -127 to 127, Physical = ((Value + 127) - 127)
    int8_t   GD_CountedBufferGamePadPointerZ;          // Usage 0x00010032: Z, Value = -127 to 127, Physical = ((Value + 127) - 127)
    int8_t   GD_CountedBufferGamePadPointerRz;         // Usage 0x00010035: Rz, Value = -127 to 127, Physical = ((Value + 127) - 127)
} HIDGameControllerDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAssistiveTouchDescriptor \
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
// Button Page HIDAssistiveTouchDescriptorInputReport (Device --> Host)
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
} HIDAssistiveTouchDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAssistiveSwitchDescriptor \
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
// Button Page HIDAssistiveSwitchDescriptorInputReport (Device --> Host)
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
} HIDAssistiveSwitchDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDHeadsetTelephonyDescriptor \
0x05, 0x0C,                  /* (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000C0001 Consumer Control (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=CA) */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x02,                  /*   (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields     */\
0x09, 0xE9,                  /*   (LOCAL)  USAGE              0x000C00E9 Volume Increment (RTC=Re-trigger Control)    */\
0x09, 0xEA,                  /*   (LOCAL)  USAGE              0x000C00EA Volume Decrement (RTC=Re-trigger Control)    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x0B,                  /*   (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x09, 0x21,                  /*   (LOCAL)  USAGE              0x000B0021 Flash (MC=Momentary Control)    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x75, 0x05,                  /*   (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x81, 0x03,                  /*   (MAIN)   INPUT              0x00000003 (1 field x 5 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Consumer Device Page HIDHeadsetTelephonyDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:2
    // Locals:  USAG:000C00EA UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C00E9 000C00EA
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    // Collection: ConsumerControl
    uint8_t  CD_ConsumerControlVolumeIncrement : 1;    // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeDecrement : 1;    // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
    
    // Field:   2
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000B0021 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000B0021
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000B: Telephony Device Page
    uint8_t  TEL_ConsumerControlFlash : 1;             // Usage 0x000B0021: Flash, Value = 0 to 1
    
    // Field:   3
    // Width:   5
    // Count:   1
    // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    ConsumerControl
    // Access:  Read/Only
    // Type:    Variable
    // Page 0x000B: Telephony Device Page
    uint8_t  : 5;                                      // Pad
} HIDHeadsetTelephonyDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDMediaPlaybackDescriptor \
0x05, 0x0C,                  /* (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page  */\
0x09, 0xB6,                  /* (LOCAL)  USAGE              0x000C00B6 Scan Previous Track (OSC=One Shot Control)  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000C0001 Consumer Control (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=CA) */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x08,                  /*   (GLOBAL) REPORT_COUNT       0x08 (8) Number of fields     */\
0x09, 0xB5,                  /*   (LOCAL)  USAGE              0x000C00B5 Scan Next Track (OSC=One Shot Control)    */\
0x09, 0xE2,                  /*   (LOCAL)  USAGE              0x000C00E2 Mute (OOC=On/Off Control)    */\
0x09, 0xB9,                  /*   (LOCAL)  USAGE              0x000C00B9 Random Play (OOC=On/Off Control)    */\
0x09, 0xBC,                  /*   (LOCAL)  USAGE              0x000C00BC Repeat (OSC=One Shot Control)    */\
0x0A, 0x5B, 0x02,            /*   (LOCAL)  USAGE              0x000C025B AC Promote (Sel=Selector)    */\
0x0A, 0x5C, 0x02,            /*   (LOCAL)  USAGE              0x000C025C AC Demote (Sel=Selector)    */\
0x0A, 0x62, 0x02,            /*   (LOCAL)  USAGE              0x000C0262 AC Add to Cart (Sel=Selector)    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (8 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Consumer Device Page HIDMediaPlaybackDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   8
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:8
    // Locals:  USAG:000C0262 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C00B5 000C00E2 000C00B9 000C00BC 000C025B 000C025C 000C0262
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    // Collection: ConsumerControl
    uint8_t  CD_ConsumerControlScanNextTrack : 1;      // Usage 0x000C00B5: Scan Next Track, Value = 0 to 1
    uint8_t  CD_ConsumerControlMute : 1;               // Usage 0x000C00E2: Mute, Value = 0 to 1
    uint8_t  CD_ConsumerControlRandomPlay : 1;         // Usage 0x000C00B9: Random Play, Value = 0 to 1
    uint8_t  CD_ConsumerControlRepeat : 1;             // Usage 0x000C00BC: Repeat, Value = 0 to 1
    uint8_t  CD_ConsumerControlAcPromote : 1;          // Usage 0x000C025B: AC Promote, Value = 0 to 1
    uint8_t  CD_ConsumerControlAcDemote : 1;           // Usage 0x000C025C: AC Demote, Value = 0 to 1
    uint8_t  CD_ConsumerControlAcAddToCart : 1;        // Usage 0x000C0262: AC Add to Cart, Value = 0 to 1
    uint8_t  CD_ConsumerControlAcAddToCart1 : 1;       // Usage 0x000C0262: AC Add to Cart, Value = 0 to 1
} HIDMediaPlaybackDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDTelephonyAndMediaPlaybackDescriptor \
0x05, 0x0C,                  /* (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000C0001 Consumer Control (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=CA) */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x0A,                  /*   (GLOBAL) REPORT_COUNT       0x0A (10) Number of fields     */\
0x09, 0xB5,                  /*   (LOCAL)  USAGE              0x000C00B5 Scan Next Track (OSC=One Shot Control)    */\
0x09, 0xB6,                  /*   (LOCAL)  USAGE              0x000C00B6 Scan Previous Track (OSC=One Shot Control)    */\
0x09, 0xE2,                  /*   (LOCAL)  USAGE              0x000C00E2 Mute (OOC=On/Off Control)    */\
0x09, 0xB9,                  /*   (LOCAL)  USAGE              0x000C00B9 Random Play (OOC=On/Off Control)    */\
0x09, 0xBC,                  /*   (LOCAL)  USAGE              0x000C00BC Repeat (OSC=One Shot Control)    */\
0x0A, 0x5B, 0x0A,            /*   (LOCAL)  USAGE              0x000C0A5B     */\
0x5C,                        /*   (ERROR)                     <-- Error: Item 5C is not a MAIN, GLOBAL or LOCAL item   */\
0x0A, 0x62, 0x09,            /*   (LOCAL)  USAGE              0x000C0962     */\
0x01, 0xA1,                  /*   (MAIN)                      <-- Error: Unknown MAIN tag   */\
0x01, 0x15,                  /*   (MAIN)                      <-- Error: Unknown MAIN tag   */\
0x00,                        /*   (MAIN)                      <-- Error: Unknown MAIN tag   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1    */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1    */\
0x95, 0x0A,                  /*   (GLOBAL) REPORT_COUNT       0x0A (10) Number of fields <-- Redundant: REPORT_COUNT is already 10    */\
0x09, 0xB5,                  /*   (LOCAL)  USAGE              0x000C00B5 Scan Next Track (OSC=One Shot Control)    */\
0x09, 0xE9,                  /*   (LOCAL)  USAGE              0x000C00E9 Volume Increment (RTC=Re-trigger Control)    */\
0x09, 0xEA,                  /*   (LOCAL)  USAGE              0x000C00EA Volume Decrement (RTC=Re-trigger Control)    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (10 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x0B,                  /*   (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x09, 0x21,                  /*   (LOCAL)  USAGE              0x000B0021 Flash (MC=Momentary Control)    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x75, 0x05,                  /*   (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x81, 0x03,                  /*   (MAIN)   INPUT              0x00000003 (1 field x 5 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Consumer Device Page HIDTelephonyAndMediaPlaybackDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   10
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:10
    // Locals:  USAG:000C00EA UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C00B5 000C00B6 000C00E2 000C00B9 000C00BC 000C0A5B 000C0962 000C00B5 000C00E9 000C00EA
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    // Collection: ConsumerControl
    uint8_t  CD_ConsumerControlScanNextTrack : 1;      // Usage 0x000C00B5: Scan Next Track, Value = 0 to 1
    uint8_t  CD_ConsumerControlScanPreviousTrack : 1;  // Usage 0x000C00B6: Scan Previous Track, Value = 0 to 1
    uint8_t  CD_ConsumerControlMute : 1;               // Usage 0x000C00E2: Mute, Value = 0 to 1
    uint8_t  CD_ConsumerControlRandomPlay : 1;         // Usage 0x000C00B9: Random Play, Value = 0 to 1
    uint8_t  CD_ConsumerControlRepeat : 1;             // Usage 0x000C00BC: Repeat, Value = 0 to 1
    uint8_t  CD_ConsumerControl0A5B : 1;               // Usage 0x000C0A5B: , Value = 0 to 1
    uint8_t  CD_ConsumerControl0962 : 1;               // Usage 0x000C0962: , Value = 0 to 1
    uint8_t  CD_ConsumerControlScanNextTrack_1 : 1;    // Usage 0x000C00B5: Scan Next Track, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeIncrement : 1;    // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeDecrement : 1;    // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
    
    // Field:   2
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000B0021 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000B0021
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000B: Telephony Device Page
    uint8_t  TEL_ConsumerControlFlash : 1;             // Usage 0x000B0021: Flash, Value = 0 to 1
    
    // Field:   3
    // Width:   5
    // Count:   1
    // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    ConsumerControl
    // Access:  Read/Only
    // Type:    Variable
    // Page 0x000B: Telephony Device Page
    uint8_t  : 5;                                      // Pad
} HIDTelephonyAndMediaPlaybackDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDSingleTouchDescriptor \
0x05, 0x0D,                  /* (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page  */\
0x09, 0x04,                  /* (LOCAL)  USAGE              0x000D0004 Touch Screen (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000D0004: Page=Digitizer Device Page, Usage=Touch Screen, Type=CA) */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34     */\
0x46, 0x64, 0x00,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0064 (100)  <-- Info: Consider replacing 46 0064 with 45 64     */\
0x55, 0x0F,                  /*     (GLOBAL) UNIT_EXPONENT      0x0F (Unit Value x 10⁻¹)      */\
0x65, 0x11,                  /*     (GLOBAL) UNIT               0x00000011 Distance in metres [1 cm units] (1=System=SI Linear, 1=Length=Centimetre)      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x07,                  /*     (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 7 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x20, 0x03,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0320 (800)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xE0, 0x01,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x01E0 (480)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDSingleTouchDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0033 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0033
    // Coll:    TouchScreen Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchScreen Finger
    uint8_t  DIG_TouchScreenFingerTouch : 1;           // Usage 0x000D0033: Touch, Value = 0 to 1, Physical = Value x 100 in 10⁻³ m units
    
    // Field:   2
    // Width:   7
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:7 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchScreen Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 7;                                      // Pad
    
    // Field:   3
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:800 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchScreen Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchScreenFingerX;                    // Usage 0x00010030: X, Value = 0 to 800, Physical = Value / 8 in 10⁻³ m units
    
    // Field:   4
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:480 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchScreen Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchScreenFingerY;                    // Usage 0x00010031: Y, Value = 0 to 480, Physical = Value x 5 / 24 in 10⁻³ m units
} HIDSingleTouchDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDButtonsDescriptor \
0x05, 0x0C,                  /* (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000C0001 Consumer Control (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=CA) */\
0x05, 0x09,                  /*   (GLOBAL) USAGE_PAGE         0x0009 Button Page    */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x0C,                  /*   (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page    */\
0x0A, 0x24, 0x02,            /*   (LOCAL)  USAGE              0x000C0224 AC Back (Sel=Selector)    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x06,                  /*   (GLOBAL) REPORT_COUNT       0x06 (6) Number of fields     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (6 fields x 1 bit) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Button Page HIDButtonsDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:00090001 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: ConsumerControl
    uint8_t  BTN_ConsumerControlButton1 : 1;           // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
    
    // Field:   2
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000C0224 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C0224
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    uint8_t  CD_ConsumerControlAcBack : 1;             // Usage 0x000C0224: AC Back, Value = 0 to 1
    
    // Field:   3
    // Width:   1
    // Count:   6
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:6
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    ConsumerControl
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000C: Consumer Device Page
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
} HIDButtonsDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDBasicGestureRecognitionDescriptor \
0x05, 0x0D,                  /* (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page  */\
0x09, 0x05,                  /* (LOCAL)  USAGE              0x000D0005 Touch Pad (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000D0005: Page=Digitizer Device Page, Usage=Touch Pad, Type=CA) */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34     */\
0x46, 0x64, 0x00,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0064 (100)  <-- Info: Consider replacing 46 0064 with 45 64     */\
0x55, 0x0F,                  /*     (GLOBAL) UNIT_EXPONENT      0x0F (Unit Value x 10⁻¹)      */\
0x65, 0x11,                  /*     (GLOBAL) UNIT               0x00000011 Distance in metres [1 cm units] (1=System=SI Linear, 1=Length=Centimetre)      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x07,                  /*     (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 7 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x00, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0400 (1024)       */\
0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34     */\
0x46, 0x64, 0x00,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0064 (100) <-- Redundant: PHYSICAL_MAXIMUM is already 100 <-- Info: Consider replacing 46 0064 with 45 64     */\
0x55, 0x0F,                  /*     (GLOBAL) UNIT_EXPONENT      0x0F (Unit Value x 10⁻¹) <-- Redundant: UNIT_EXPONENT is already -1     */\
0x65, 0x11,                  /*     (GLOBAL) UNIT               0x00000011 Distance in metres [1 cm units] (1=System=SI Linear, 1=Length=Centimetre) <-- Redundant: UNIT is already 0x00000011     */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x00, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0400 (1024) <-- Redundant: LOGICAL_MAXIMUM is already 1024      */\
0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34     */\
0x46, 0x64, 0x00,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0064 (100) <-- Redundant: PHYSICAL_MAXIMUM is already 100 <-- Info: Consider replacing 46 0064 with 45 64     */\
0x55, 0x0F,                  /*     (GLOBAL) UNIT_EXPONENT      0x0F (Unit Value x 10⁻¹) <-- Redundant: UNIT_EXPONENT is already -1     */\
0x65, 0x11,                  /*     (GLOBAL) UNIT               0x00000011 Distance in metres [1 cm units] (1=System=SI Linear, 1=Length=Centimetre) <-- Redundant: UNIT is already 0x00000011     */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
0x09, 0x24,                  /*   (LOCAL)  USAGE              0x000D0024     */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0024: Page=Digitizer Device Page, Usage=, Type=) <-- Warning: USAGE type should be CL (Logical)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x63,                  /*     (LOCAL)  USAGE              0x000D0063       */\
0x75, 0x20,                  /*     (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x82, 0x02, 0x01,            /*     (MAIN)   INPUT              0x00000102 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 1=Buffer      */\
0x09, 0x65,                  /*     (LOCAL)  USAGE              0x000D0065       */\
0x09, 0x62,                  /*     (LOCAL)  USAGE              0x000D0062       */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap  <-- Error: REPORT_SIZE (8) is too small for LOGICAL_MAXIMUM (1024) which needs 11 bits.     */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDBasicGestureRecognitionDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0033 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0033
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchPad Finger
    uint8_t  DIG_TouchPadFingerTouch : 1;              // Usage 0x000D0033: Touch, Value = 0 to 1, Physical = Value x 100 in 10⁻³ m units
    
    // Field:   2
    // Width:   7
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:7 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 7;                                      // Pad
    
    // Field:   3
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:1024 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX;                       // Usage 0x00010030: X, Value = 0 to 1024, Physical = Value x 25 / 256 in 10⁻³ m units
    
    // Field:   4
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:1024 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY;                       // Usage 0x00010031: Y, Value = 0 to 1024, Physical = Value x 25 / 256 in 10⁻³ m units
    
    // Field:   5
    // Width:   32
    // Count:   1
    // Flags:   00000102: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 1=Buffer
    // Globals: PAGE:000D LMIN:0 LMAX:1024 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:32 RID:0 RCNT:1
    // Locals:  USAG:000D0063 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0063
    // Coll:    TouchPad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchPad
    uint32_t DIG_TouchPad0063;                         // Usage 0x000D0063: , Value = 0 to 1024, Physical = Value x 25 / 256 in 10⁻³ m units
    
    // Field:   6
    // Width:   8
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1024 PMIN:0 PMAX:100 UEXP:-1 UNIT:00000011 RSIZ:8 RID:0 RCNT:2
    // Locals:  USAG:000D0062 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0065 000D0062
    // Coll:    TouchPad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPad0065;                         // Usage 0x000D0065: , Value = 0 to 1024, Physical = Value x 25 / 256 in 10⁻³ m units
    uint8_t  DIG_TouchPad0062;                         // Usage 0x000D0062: , Value = 0 to 1024, Physical = Value x 25 / 256 in 10⁻³ m units
} HIDBasicGestureRecognitionDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDGestureRecognitionAndAltInterpretationDescriptor \
0x05, 0x0D,                  /* (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page  */\
0x09, 0x05,                  /* (LOCAL)  USAGE              0x000D0005 Touch Pad (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000D0005: Page=Digitizer Device Page, Usage=Touch Pad, Type=CA) */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x07,                  /*     (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 7 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x00, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0400 (1024)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x00, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0400 (1024) <-- Redundant: LOGICAL_MAXIMUM is already 1024      */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
0x09, 0x23,                  /*   (LOCAL)  USAGE              0x000D0023 Device Settings (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0023: Page=Digitizer Device Page, Usage=Device Settings, Type=CL)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x60,                  /*     (LOCAL)  USAGE              0x000D0060       */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0xB1, 0x02,                  /*     (MAIN)   FEATURE            0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x75, 0x07,                  /*     (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field       */\
0xB1, 0x01,                  /*     (MAIN)   FEATURE            0x00000001 (1 field x 7 bits) 1=Constant 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x24,                  /*   (LOCAL)  USAGE              0x000D0024     */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0024: Page=Digitizer Device Page, Usage=, Type=) <-- Warning: USAGE type should be CL (Logical)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x63,                  /*     (LOCAL)  USAGE              0x000D0063       */\
0x75, 0x20,                  /*     (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x82, 0x02, 0x01,            /*     (MAIN)   INPUT              0x00000102 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 1=Buffer      */\
0x09, 0x62,                  /*     (LOCAL)  USAGE              0x000D0062       */\
0x09, 0x66,                  /*     (LOCAL)  USAGE              0x000D0066       */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x61,                  /*     (LOCAL)  USAGE              0x000D0061       */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x64,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x64 (100)       */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x24,                  /*   (LOCAL)  USAGE              0x000D0024     */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0024: Page=Digitizer Device Page, Usage=, Type=) <-- Warning: USAGE type should be CL (Logical)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x63,                  /*     (LOCAL)  USAGE              0x000D0063       */\
0x75, 0x20,                  /*     (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x82, 0x02, 0x01,            /*     (MAIN)   INPUT              0x00000102 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 1=Buffer      */\
0x09, 0x62,                  /*     (LOCAL)  USAGE              0x000D0062       */\
0x09, 0x66,                  /*     (LOCAL)  USAGE              0x000D0066       */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x61,                  /*     (LOCAL)  USAGE              0x000D0061       */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x64,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x64 (100) <-- Redundant: LOGICAL_MAXIMUM is already 100      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDGestureRecognitionAndAltInterpretationDescriptorFeatureReport (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0060 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0060
    // Coll:    TouchPad DeviceSettings
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchPad DeviceSettings
    uint8_t  DIG_TouchPadDeviceSettings0060 : 1;       // Usage 0x000D0060: , Value = 0 to 1
    
    // Field:   2
    // Width:   7
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:7 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad DeviceSettings
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 7;                                      // Pad
} HIDGestureRecognitionAndAltInterpretationDescriptorFeatureReport;


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDGestureRecognitionAndAltInterpretationDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0033 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0033
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchPad Finger
    uint8_t  DIG_TouchPadFingerTouch : 1;              // Usage 0x000D0033: Touch, Value = 0 to 1
    
    // Field:   2
    // Width:   7
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:7 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 7;                                      // Pad
    
    // Field:   3
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:1024 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX;                       // Usage 0x00010030: X, Value = 0 to 1024
    
    // Field:   4
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:1024 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY;                       // Usage 0x00010031: Y, Value = 0 to 1024
    
    // Field:   5
    // Width:   32
    // Count:   1
    // Flags:   00000102: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 1=Buffer
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:0 RCNT:1
    // Locals:  USAG:000D0063 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0063
    // Coll:    TouchPad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchPad
    uint32_t DIG_TouchPad0063;                         // Usage 0x000D0063: , Value = 0 to 1
    
    // Field:   6
    // Width:   8
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:2
    // Locals:  USAG:000D0066 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0062 000D0066
    // Coll:    TouchPad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPad0062;                         // Usage 0x000D0062: , Value = 0 to 1
    uint8_t  DIG_TouchPad0066;                         // Usage 0x000D0066: , Value = 0 to 1
    
    // Field:   7
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:100 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0061 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0061
    // Coll:    TouchPad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPad0061;                         // Usage 0x000D0061: , Value = 0 to 100
    
    // Field:   8
    // Width:   32
    // Count:   1
    // Flags:   00000102: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 1=Buffer
    // Globals: PAGE:000D LMIN:0 LMAX:100 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:0 RCNT:1
    // Locals:  USAG:000D0063 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0063
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection:
    uint32_t DIG_VendorDefined0063;                    // Usage 0x000D0063: , Value = 0 to 100
    
    // Field:   9
    // Width:   8
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:100 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:2
    // Locals:  USAG:000D0066 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0062 000D0066
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_VendorDefined0062;                    // Usage 0x000D0062: , Value = 0 to 100
    uint8_t  DIG_VendorDefined0066;                    // Usage 0x000D0066: , Value = 0 to 100
    
    // Field:   10
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:100 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0061 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0061
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_VendorDefined0061;                    // Usage 0x000D0061: , Value = 0 to 100
} HIDGestureRecognitionAndAltInterpretationDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDMultiAxisControllerDescriptor \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x08,                  /* (LOCAL)  USAGE              0x00010008 Multi-axis Controller (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010008: Page=Generic Desktop Page, Usage=Multi-axis Controller, Type=CA) */\
0x05, 0x09,                  /*   (GLOBAL) USAGE_PAGE         0x0009 Button Page    */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x0C,                  /*   (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page    */\
0x0A, 0x23, 0x00,            /*   (LOCAL)  USAGE              0x000C0023     */\
0x0A, 0x24, 0x00,            /*   (LOCAL)  USAGE              0x000C0024     */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1    */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1    */\
0x95, 0x02,                  /*   (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x75, 0x05,                  /*   (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x05, 0x01,                  /*   (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page    */\
0x09, 0x30,                  /*   (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)    */\
0x09, 0x31,                  /*   (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)    */\
0x15, 0x81,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x81 (-127)     */\
0x25, 0x7F,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x7F (127)     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
0x95, 0x02,                  /*   (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (2 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x01,                  /*   (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page <-- Redundant: USAGE_PAGE is already 0x0001   */\
0x09, 0x38,                  /*   (LOCAL)  USAGE              0x00010038 Wheel (DV=Dynamic Value)    */\
0x15, 0x81,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x81 (-127) <-- Redundant: LOGICAL_MINIMUM is already -127    */\
0x25, 0x7F,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x7F (127) <-- Redundant: LOGICAL_MAXIMUM is already 127    */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x06,                  /*   (MAIN)   INPUT              0x00000006 (1 field x 8 bits) 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Button Page HIDMultiAxisControllerDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:00090001 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001
    // Coll:    Multi-axisController
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: Multi-axisController
    uint8_t  BTN_MultiaxisControllerButton1 : 1;       // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
    
    // Field:   2
    // Width:   1
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:2
    // Locals:  USAG:000C0024 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C0023 000C0024
    // Coll:    Multi-axisController
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    uint8_t  CD_MultiaxisController0023 : 1;           // Usage 0x000C0023: , Value = 0 to 1
    uint8_t  CD_MultiaxisController0024 : 1;           // Usage 0x000C0024: , Value = 0 to 1
    
    // Field:   3
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Multi-axisController
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000C: Consumer Device Page
    uint8_t  : 5;                                      // Pad
    
    // Field:   4
    // Width:   8
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:-127 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:2
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030 00010031
    // Coll:    Multi-axisController
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    int8_t   GD_MultiaxisControllerX;                  // Usage 0x00010030: X, Value = -127 to 127
    int8_t   GD_MultiaxisControllerY;                  // Usage 0x00010031: Y, Value = -127 to 127
    
    // Field:   5
    // Width:   8
    // Count:   1
    // Flags:   00000006: 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:-127 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:00010038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010038
    // Coll:    Multi-axisController
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    int8_t   GD_MultiaxisControllerWheel;              // Usage 0x00010038: Wheel, Value = -127 to 127
} HIDMultiAxisControllerDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDSimpleMediaButtonsDescriptor \
0x05, 0x0C,                  /* (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000C0001 Consumer Control (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=CA) */\
0x05, 0x0C,                  /*   (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page <-- Redundant: USAGE_PAGE is already 0x000C   */\
0x09, 0xB5,                  /*   (LOCAL)  USAGE              0x000C00B5 Scan Next Track (OSC=One Shot Control)    */\
0x09, 0xB6,                  /*   (LOCAL)  USAGE              0x000C00B6 Scan Previous Track (OSC=One Shot Control)    */\
0x09, 0xCD,                  /*   (LOCAL)  USAGE              0x000C00CD Play/Pause (OSC=One Shot Control)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x03,                  /*   (GLOBAL) REPORT_COUNT       0x03 (3) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (3 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x75, 0x05,                  /*   (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Consumer Device Page HIDSimpleMediaButtonsDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   3
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:3
    // Locals:  USAG:000C00CD UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C00B5 000C00B6 000C00CD
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    // Collection: ConsumerControl
    uint8_t  CD_ConsumerControlScanNextTrack : 1;      // Usage 0x000C00B5: Scan Next Track, Value = 0 to 1
    uint8_t  CD_ConsumerControlScanPreviousTrack : 1;  // Usage 0x000C00B6: Scan Previous Track, Value = 0 to 1
    uint8_t  CD_ConsumerControlPlayPause : 1;          // Usage 0x000C00CD: Play/Pause, Value = 0 to 1
    
    // Field:   2
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    ConsumerControl
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000C: Consumer Device Page
    uint8_t  : 5;                                      // Pad
} HIDSimpleMediaButtonsDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDTelephonyButtonsFlashNumpadDescriptor \
0x05, 0x0B,                  /* (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000B0001 Phone (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000B0001: Page=Telephony Device Page, Usage=Phone, Type=CA) */\
0x05, 0x0B,                  /*   (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page <-- Redundant: USAGE_PAGE is already 0x000B   */\
0x09, 0x21,                  /*   (LOCAL)  USAGE              0x000B0021 Flash (MC=Momentary Control)    */\
0x09, 0x2F,                  /*   (LOCAL)  USAGE              0x000B002F Phone Mute (OOC=On/Off Control)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x02,                  /*   (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x75, 0x06,                  /*   (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 6 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x05, 0x0B,                  /*   (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page <-- Redundant: USAGE_PAGE is already 0x000B   */\
0x09, 0xB0,                  /*   (LOCAL)  USAGE              0x000B00B0 Phone Key 0 (Sel=Selector)    */\
0x09, 0xB1,                  /*   (LOCAL)  USAGE              0x000B00B1 Phone Key 1 (Sel=Selector)    */\
0x09, 0xB2,                  /*   (LOCAL)  USAGE              0x000B00B2 Phone Key 2 (Sel=Selector)    */\
0x09, 0xB3,                  /*   (LOCAL)  USAGE              0x000B00B3 Phone Key 3 (Sel=Selector)    */\
0x09, 0xB4,                  /*   (LOCAL)  USAGE              0x000B00B4 Phone Key 4 (Sel=Selector)    */\
0x09, 0xB5,                  /*   (LOCAL)  USAGE              0x000B00B5 Phone Key 5 (Sel=Selector)    */\
0x09, 0xB6,                  /*   (LOCAL)  USAGE              0x000B00B6 Phone Key 6 (Sel=Selector)    */\
0x09, 0xB7,                  /*   (LOCAL)  USAGE              0x000B00B7 Phone Key 7 (Sel=Selector)    */\
0x09, 0xB8,                  /*   (LOCAL)  USAGE              0x000B00B8 Phone Key 8 (Sel=Selector)    */\
0x09, 0xB9,                  /*   (LOCAL)  USAGE              0x000B00B9 Phone Key 9 (Sel=Selector)    */\
0x09, 0xBA,                  /*   (LOCAL)  USAGE              0x000B00BA Phone Key * (Sel=Selector)    */\
0x09, 0xBB,                  /*   (LOCAL)  USAGE              0x000B00BB Phone Key # (Sel=Selector)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1    */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x0C,                  /*   (GLOBAL) REPORT_COUNT       0x0C (12) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (12 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x75, 0x04,                  /*   (GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 4 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Telephony Device Page HIDTelephonyButtonsFlashNumpadDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:2
    // Locals:  USAG:000B002F UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000B0021 000B002F
    // Coll:    Phone
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000B: Telephony Device Page
    // Collection: Phone
    uint8_t  TEL_PhoneFlash : 1;                       // Usage 0x000B0021: Flash, Value = 0 to 1
    uint8_t  TEL_PhonePhoneMute : 1;                   // Usage 0x000B002F: Phone Mute, Value = 0 to 1
    
    // Field:   2
    // Width:   6
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:6 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Phone
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000B: Telephony Device Page
    uint8_t  : 6;                                      // Pad
    
    // Field:   3
    // Width:   1
    // Count:   12
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:12
    // Locals:  USAG:000B00BB UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000B00B0 000B00B1 000B00B2 000B00B3 000B00B4 000B00B5 000B00B6 000B00B7 000B00B8 000B00B9 000B00BA 000B00BB
    // Coll:    Phone
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000B: Telephony Device Page
    uint8_t  TEL_PhonePhoneKey0 : 1;                   // Usage 0x000B00B0: Phone Key 0, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey1 : 1;                   // Usage 0x000B00B1: Phone Key 1, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey2 : 1;                   // Usage 0x000B00B2: Phone Key 2, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey3 : 1;                   // Usage 0x000B00B3: Phone Key 3, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey4 : 1;                   // Usage 0x000B00B4: Phone Key 4, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey5 : 1;                   // Usage 0x000B00B5: Phone Key 5, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey6 : 1;                   // Usage 0x000B00B6: Phone Key 6, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey7 : 1;                   // Usage 0x000B00B7: Phone Key 7, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey8 : 1;                   // Usage 0x000B00B8: Phone Key 8, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey9 : 1;                   // Usage 0x000B00B9: Phone Key 9, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey : 1;                    // Usage 0x000B00BA: Phone Key *, Value = 0 to 1
    uint8_t  TEL_PhonePhoneKey_1 : 1;                  // Usage 0x000B00BB: Phone Key #, Value = 0 to 1
    
    // Field:   4
    // Width:   4
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000B LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:4 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Phone
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000B: Telephony Device Page
    uint8_t  : 4;                                      // Pad
} HIDTelephonyButtonsFlashNumpadDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDVendorMessage32BitDescriptor \
0x06, 0x00, 0xFF,            /* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined  */\
0x09, 0x23,                  /* (LOCAL)  USAGE              0xFF000023   */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=) */\
0x09, 0x23,                  /*   (LOCAL)  USAGE              0xFF000023     */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Vendor-defined HIDVendorMessage32BitDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:0 RCNT:1
    // Locals:  USAG:FF000023 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000023
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF00: Vendor-defined
    uint32_t VEN_VendorDefined0023;                    // Usage 0xFF000023: , Value = 0 to 0
} HIDVendorMessage32BitDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDVendorDescriptor \
0x06, 0x00, 0xFF,            /* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined  */\
0x09, 0x80,                  /* (LOCAL)  USAGE              0xFF000080   */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0xFF000080: Page=Vendor-defined, Usage=, Type=) */\
0x09, 0x81,                  /*   (LOCAL)  USAGE              0xFF000081     */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x09, 0x82,                  /*   (LOCAL)  USAGE              0xFF000082     */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
0x91, 0x02,                  /*   (MAIN)   OUTPUT             0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x09, 0x83,                  /*   (LOCAL)  USAGE              0xFF000083     */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Vendor-defined HIDVendorDescriptorFeatureReport (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:0 RCNT:1
    // Locals:  USAG:FF000083 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000083
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF00: Vendor-defined
    uint32_t VEN_VendorDefined0083;                    // Usage 0xFF000083: , Value = 0 to 0
} HIDVendorDescriptorFeatureReport;


//--------------------------------------------------------------------------------
// Vendor-defined HIDVendorDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:0 RCNT:1
    // Locals:  USAG:FF000081 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000081
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF00: Vendor-defined
    uint32_t VEN_VendorDefined0081;                    // Usage 0xFF000081: , Value = 0 to 0
} HIDVendorDescriptorInputReport;


//--------------------------------------------------------------------------------
// Vendor-defined HIDVendorDescriptorOutputReport (Device <-- Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:0 RCNT:1
    // Locals:  USAG:FF000082 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000082
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF00: Vendor-defined
    uint32_t VEN_VendorDefined0082;                    // Usage 0xFF000082: , Value = 0 to 0
} HIDVendorDescriptorOutputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDDigitizerWithTouchCancel \
0x05, 0x0D,                  /* (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page  */\
0x09, 0x05,                  /* (LOCAL)  USAGE              0x000D0005 Touch Pad (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000D0005: Page=Digitizer Device Page, Usage=Touch Pad, Type=CA) */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x09, 0x34,                  /*     (LOCAL)  USAGE              0x000D0034 Untouch (OSC=One Shot Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x06,                  /*     (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 6 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x00, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0400 (1024)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x00, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0400 (1024) <-- Redundant: LOGICAL_MAXIMUM is already 1024      */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDDigitizerWithTouchCancelInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0038
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchPad Finger
    uint8_t  DIG_TouchPadFingerTransducerIndex;        // Usage 0x000D0038: Transducer Index, Value = 0 to 0
    
    // Field:   2
    // Width:   1
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:2
    // Locals:  USAG:000D0034 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0033 000D0034
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTouch : 1;              // Usage 0x000D0033: Touch, Value = 0 to 1
    uint8_t  DIG_TouchPadFingerUntouch : 1;            // Usage 0x000D0034: Untouch, Value = 0 to 1
    
    // Field:   3
    // Width:   6
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:6 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 6;                                      // Pad
    
    // Field:   4
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:1024 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX;                       // Usage 0x00010030: X, Value = 0 to 1024
    
    // Field:   5
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:1024 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY;                       // Usage 0x00010031: Y, Value = 0 to 1024
} HIDDigitizerWithTouchCancelInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDDigitizerForHangdog \
0x05, 0x0D,                  /* (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page  */\
0x09, 0x05,                  /* (LOCAL)  USAGE              0x000D0005 Touch Pad (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000D0005: Page=Digitizer Device Page, Usage=Touch Pad, Type=CA) */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x07,                  /*     (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 7 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x20, 0x03,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0320 (800)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xE0, 0x01,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x01E0 (480)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x05, 0x0D,                  /*     (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D     */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap  <-- Error: REPORT_SIZE (8) is too small for LOGICAL_MAXIMUM (480) which needs 9 bits.     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x07,                  /*     (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 7 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0x20, 0x03,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0320 (800)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xE0, 0x01,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x01E0 (480)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x06, 0x60, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF60 Vendor-defined    */\
0x09, 0x05,                  /*   (LOCAL)  USAGE              0xFF600005     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap  <-- Error: REPORT_SIZE (1) is too small for LOGICAL_MAXIMUM (480) which needs 9 bits.   */\
0x75, 0x07,                  /*   (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDDigitizerForHangdogInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0038
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    // Collection: TouchPad Finger
    uint8_t  DIG_TouchPadFingerTransducerIndex;        // Usage 0x000D0038: Transducer Index, Value = 0 to 0
    
    // Field:   2
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0033 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0033
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTouch : 1;              // Usage 0x000D0033: Touch, Value = 0 to 1
    
    // Field:   3
    // Width:   7
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:7 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 7;                                      // Pad
    
    // Field:   4
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:800 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX;                       // Usage 0x00010030: X, Value = 0 to 800
    
    // Field:   5
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:480 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY;                       // Usage 0x00010031: Y, Value = 0 to 480
    
    // Field:   6
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:480 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0038
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTransducerIndex_1;      // Usage 0x000D0038: Transducer Index, Value = 0 to 480
    
    // Field:   7
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0033 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0033
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTouch_1 : 1;            // Usage 0x000D0033: Touch, Value = 0 to 1
    
    // Field:   8
    // Width:   7
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:7 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 7;                                      // Pad
    
    // Field:   9
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:800 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX_1;                     // Usage 0x00010030: X, Value = 0 to 800
    
    // Field:   10
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:480 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY_1;                     // Usage 0x00010031: Y, Value = 0 to 480
    
    // Field:   11
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF60 LMIN:0 LMAX:480 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:FF600005 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF600005
    // Coll:    TouchPad
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF60: Vendor-defined
    // Collection: TouchPad
    uint8_t  VEN_TouchPad0005 : 1;                     // Usage 0xFF600005: , Value = 0 to 480
} HIDDigitizerForHangdogInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDMouseDescriptor \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x02,                  /* (LOCAL)  USAGE              0x00010002 Mouse (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010002: Page=Generic Desktop Page, Usage=Mouse, Type=CA) */\
0x05, 0x09,                  /*   (GLOBAL) USAGE_PAGE         0x0009 Button Page    */\
0x19, 0x01,                  /*   (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
0x29, 0x04,                  /*   (LOCAL)  USAGE_MAXIMUM      0x00090004 Button 4 (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x95, 0x04,                  /*   (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (4 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x04,                  /*   (GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 4 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x05, 0x01,                  /*   (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page    */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0x00010001 Pointer (CP=Physical Collection)    */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=CP)   */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x09, 0x32,                  /*     (LOCAL)  USAGE              0x00010032 Z (DV=Dynamic Value)      */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x00010038 Wheel (DV=Dynamic Value)      */\
0x15, 0x81,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x81 (-127)       */\
0x25, 0x7F,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x7F (127)       */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x04,                  /*     (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields       */\
0x81, 0x06,                  /*     (MAIN)   INPUT              0x00000006 (4 fields x 8 bits) 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0x05, 0xFF,                  /*   (GLOBAL) USAGE_PAGE         0x00FF Reserved    */\
0x09, 0xC0,                  /*   (LOCAL)  USAGE              0x00FF00C0     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Button Page HIDMouseDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:4
    // Locals:  USAG:0 UMIN:00090001 UMAX:00090004 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001 00090002 00090003 00090004
    // Coll:    Mouse
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: Mouse
    uint8_t  BTN_MouseButton1 : 1;                     // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
    uint8_t  BTN_MouseButton2 : 1;                     // Usage 0x00090002: Button 2 Secondary, Value = 0 to 1
    uint8_t  BTN_MouseButton3 : 1;                     // Usage 0x00090003: Button 3 Tertiary, Value = 0 to 1
    uint8_t  BTN_MouseButton4 : 1;                     // Usage 0x00090004: Button 4, Value = 0 to 1
    
    // Field:   2
    // Width:   4
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:4 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Mouse
    // Access:  Read/Only
    // Type:    Array
    // Page 0x0009: Button Page
    uint8_t  : 4;                                      // Pad
    
    // Field:   3
    // Width:   8
    // Count:   4
    // Flags:   00000006: 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:-127 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:4
    // Locals:  USAG:00010038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030 00010031 00010032 00010038
    // Coll:    Mouse Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    // Collection: Mouse Pointer
    int8_t   GD_MousePointerX;                         // Usage 0x00010030: X, Value = -127 to 127
    int8_t   GD_MousePointerY;                         // Usage 0x00010031: Y, Value = -127 to 127
    int8_t   GD_MousePointerZ;                         // Usage 0x00010032: Z, Value = -127 to 127
    int8_t   GD_MousePointerWheel;                     // Usage 0x00010038: Wheel, Value = -127 to 127
    
    // Field:   4
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:00FF LMIN:-127 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:00FF00C0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00FF00C0
    // Coll:    Mouse
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x00FF: Reserved
    // Collection: Mouse
    int8_t   RES_Mouse00C0;                            // Usage 0x00FF00C0: , Value = -127 to 127
} HIDMouseDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDPointerAbsoluteDescriptor \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x02,                  /* (LOCAL)  USAGE              0x00010002 Mouse (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010002: Page=Generic Desktop Page, Usage=Mouse, Type=CA) */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0x00010001 Pointer (CP=Physical Collection)    */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=CP)   */\
0x05, 0x09,                  /*     (GLOBAL) USAGE_PAGE         0x0009 Button Page      */\
0x19, 0x01,                  /*     (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x29, 0x10,                  /*     (LOCAL)  USAGE_MAXIMUM      0x00090010 Button 16 (MULTI=Selector, On/Off, Momentary, or One Shot)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x10,                  /*     (GLOBAL) REPORT_COUNT       0x10 (16) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (16 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x27, 0xFF, 0x7F, 0x00, 0x00, /*     (GLOBAL) LOGICAL_MAXIMUM    0x00007FFF (32767)  <-- Info: Consider replacing 27 00007FFF with 26 7FFF     */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x00010038 Wheel (DV=Dynamic Value)      */\
0x15, 0x81,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x81 (-127)       */\
0x25, 0x7F,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x7F (127)       */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x81, 0x06,                  /*     (MAIN)   INPUT              0x00000006 (1 field x 8 bits) 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x05, 0x0C,                  /*     (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page      */\
0x0A, 0x38, 0x02,            /*     (LOCAL)  USAGE              0x000C0238 AC Pan (LC=Linear Control)      */\
0x81, 0x06,                  /*     (MAIN)   INPUT              0x00000006 (1 field x 8 bits) 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Button Page HIDPointerAbsoluteDescriptorInputReport (Device --> Host)
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
    // Coll:    Mouse Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: Mouse Pointer
    uint8_t  BTN_MousePointerButton1 : 1;              // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
    uint8_t  BTN_MousePointerButton2 : 1;              // Usage 0x00090002: Button 2 Secondary, Value = 0 to 1
    uint8_t  BTN_MousePointerButton3 : 1;              // Usage 0x00090003: Button 3 Tertiary, Value = 0 to 1
    uint8_t  BTN_MousePointerButton4 : 1;              // Usage 0x00090004: Button 4, Value = 0 to 1
    uint8_t  BTN_MousePointerButton5 : 1;              // Usage 0x00090005: Button 5, Value = 0 to 1
    uint8_t  BTN_MousePointerButton6 : 1;              // Usage 0x00090006: Button 6, Value = 0 to 1
    uint8_t  BTN_MousePointerButton7 : 1;              // Usage 0x00090007: Button 7, Value = 0 to 1
    uint8_t  BTN_MousePointerButton8 : 1;              // Usage 0x00090008: Button 8, Value = 0 to 1
    uint8_t  BTN_MousePointerButton9 : 1;              // Usage 0x00090009: Button 9, Value = 0 to 1
    uint8_t  BTN_MousePointerButton10 : 1;             // Usage 0x0009000A: Button 10, Value = 0 to 1
    uint8_t  BTN_MousePointerButton11 : 1;             // Usage 0x0009000B: Button 11, Value = 0 to 1
    uint8_t  BTN_MousePointerButton12 : 1;             // Usage 0x0009000C: Button 12, Value = 0 to 1
    uint8_t  BTN_MousePointerButton13 : 1;             // Usage 0x0009000D: Button 13, Value = 0 to 1
    uint8_t  BTN_MousePointerButton14 : 1;             // Usage 0x0009000E: Button 14, Value = 0 to 1
    uint8_t  BTN_MousePointerButton15 : 1;             // Usage 0x0009000F: Button 15, Value = 0 to 1
    uint8_t  BTN_MousePointerButton16 : 1;             // Usage 0x00090010: Button 16, Value = 0 to 1
    
    // Field:   2
    // Width:   16
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:2
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030 00010031
    // Coll:    Mouse Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_MousePointerX;                         // Usage 0x00010030: X, Value = 0 to 32767
    uint16_t GD_MousePointerY;                         // Usage 0x00010031: Y, Value = 0 to 32767
    
    // Field:   3
    // Width:   8
    // Count:   1
    // Flags:   00000006: 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:-127 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:00010038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010038
    // Coll:    Mouse Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    int8_t   GD_MousePointerWheel;                     // Usage 0x00010038: Wheel, Value = -127 to 127
    
    // Field:   4
    // Width:   8
    // Count:   1
    // Flags:   00000006: 0=Data 1=Variable 1=Relative 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:-127 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000C0238 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C0238
    // Coll:    Mouse Pointer
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    int8_t   CD_MousePointerAcPan;                     // Usage 0x000C0238: AC Pan, Value = -127 to 127
} HIDPointerAbsoluteDescriptorInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDBiometricHumanPresenceAndProximity \
0x05, 0x20,                  /* (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
0x09, 0x11,                  /* (LOCAL)  USAGE              0x00200011 Biometric: Human Presence (CACP=Application or Physical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00200011: Page=Sensor Device Page, Usage=Biometric: Human Presence, Type=CACP) */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page <-- Redundant: USAGE_PAGE is already 0x0020   */\
0x0A, 0xB1, 0x04,            /*   (LOCAL)  USAGE              0x002004B1 Data Field: Human Presence (SF=Static Flag)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Sensor Device Page HIDBiometricHumanPresenceAndProximityInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:002004B1 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  002004B1
    // Coll:    Biometric:HumanPresence
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    // Collection: Biometric:HumanPresence
    uint8_t  SNS_BiometricHumanPresenceDataFieldHumanPresence; // Usage 0x002004B1: Data Field: Human Presence, Value = 0 to 1
} HIDBiometricHumanPresenceAndProximityInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

//#define HIDAudioButtons \
//0x05, 0x0B,                  /* (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page  */\
//0x09, 0x05,                  /* (LOCAL)  USAGE              0x000B0005 Headset (CL=Logical Collection)  */\
//0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000B0005: Page=Telephony Device Page, Usage=Headset, Type=CL) <-- Warning: USAGE type should be CA (Application) */\
//0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAudioButtons \
0x05, 0x0C,                  /* (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000C0001 Consumer Control (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=CA) */\
0x05, 0x0B,                  /*   (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page    */\
0x09, 0x21,                  /*   (LOCAL)  USAGE              0x000B0021 Flash (MC=Momentary Control)    */\
0x05, 0x0C,                  /*   (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page    */\
0x09, 0xEA,                  /*   (LOCAL)  USAGE              0x000C00EA Volume Decrement (RTC=Re-trigger Control)    */\
0x09, 0xE9,                  /*   (LOCAL)  USAGE              0x000C00E9 Volume Increment (RTC=Re-trigger Control)    */\
0x06, 0x07, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF07 Vendor-defined    */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0xFF070001     */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x95, 0x04,                  /*   (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (4 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */\
0x81, 0x05,                  /*   (MAIN)   INPUT              0x00000005 (5 fields x 1 bit) 1=Constant 0=Array 1=Relative 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Vendor-defined HIDAudioButtonsInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF07 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:4
    // Locals:  USAG:FF070001 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000B0021 000C00EA 000C00E9 FF070001
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF07: Vendor-defined
    // Collection: ConsumerControl
    uint8_t  TEL_ConsumerControlFlash : 1;             // Usage 0x000B0021: Flash, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeDecrement : 1;    // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeIncrement : 1;    // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
    uint8_t  VEN_ConsumerControl0001 : 1;              // Usage 0xFF070001: , Value = 0 to 1
    
    // Field:   2
    // Width:   1
    // Count:   5
    // Flags:   00000005: 1=Constant 0=Array 1=Relative 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:FF07 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:5
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    ConsumerControl
    // Access:  Read/Only
    // Type:    Array
    // Page 0xFF07: Vendor-defined
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
} HIDAudioButtonsInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDCarplaySelectButton \
0x05, 0x0C,                  /* (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000C0001 Consumer Control (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=CA) */\
0x05, 0x09,                  /*   (GLOBAL) USAGE_PAGE         0x0009 Button Page    */\
0x09, 0x01,                  /*   (LOCAL)  USAGE              0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x0C,                  /*   (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page    */\
0x0A, 0x24, 0x02,            /*   (LOCAL)  USAGE              0x000C0224 AC Back (Sel=Selector)    */\
0x0A, 0x23, 0x02,            /*   (LOCAL)  USAGE              0x000C0223 AC Home (Sel=Selector)    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (5 fields x 1 bit) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Button Page HIDCarplaySelectButtonInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:00090001 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00090001
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0009: Button Page
    // Collection: ConsumerControl
    uint8_t  BTN_ConsumerControlButton1 : 1;           // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
    
    // Field:   2
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000C0223 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C0224 000C0223
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    uint8_t  CD_ConsumerControlAcBack : 1;             // Usage 0x000C0224: AC Back, Value = 0 to 1
    // Usage 0x000C0223 AC Home (Sel=Selector) Value = 0 to 1 <-- Ignored: REPORT_COUNT (1) is too small
    
    // Field:   3
    // Width:   1
    // Count:   5
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:5
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    ConsumerControl
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000C: Consumer Device Page
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
    uint8_t  : 1;                                      // Pad
} HIDCarplaySelectButtonInputReport;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAccel \
0x05, 0x20,                  /* (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
0x09, 0x73,                  /* (LOCAL)  USAGE              0x00200073 Motion: Accelerometer 3D (CACP=Application or Physical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00200073: Page=Sensor Device Page, Usage=Motion: Accelerometer 3D, Type=CACP) */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
0x46, 0xFF, 0x7F,            /*   (GLOBAL) PHYSICAL_MAXIMUM   0x7FFF (32767)     */\
0x36, 0x00, 0x80,            /*   (GLOBAL) PHYSICAL_MINIMUM   0x8000 (-32768)     */\
0x26, 0xFF, 0x7F,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)     */\
0x16, 0x00, 0x80,            /*   (GLOBAL) LOGICAL_MINIMUM    0x8000 (-32768)     */\
0x55, 0x0D,                  /*   (GLOBAL) UNIT_EXPONENT      0x0D (Unit Value x 10⁻³)    */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page <-- Redundant: USAGE_PAGE is already 0x0020   */\
0x0A, 0x53, 0x04,            /*   (LOCAL)  USAGE              0x00200453 Data Field: Acceleration Axis X (SV=Static Value)    */\
0x75, 0x10,                  /*   (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x0A, 0x54, 0x04,            /*   (LOCAL)  USAGE              0x00200454 Data Field: Acceleration Axis Y (SV=Static Value)    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page <-- Redundant: USAGE_PAGE is already 0x0020   */\
0x0A, 0x55, 0x04,            /*   (LOCAL)  USAGE              0x00200455 Data Field: Acceleration Axis Z (SV=Static Value)    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x06, 0x0C, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF0C     */\
0x09, 0x09,                  /*   (LOCAL)  USAGE              0xFF0C0009     */\
0x24,                        /*   (GLOBAL) LOGICAL_MAXIMUM    (0)     */\
0x14,                        /*   (GLOBAL) LOGICAL_MINIMUM    (0)     */\
0x45, 0x02,                  /*   (GLOBAL) PHYSICAL_MAXIMUM   0x02 (2)     */\
0x34,                        /*   (GLOBAL) PHYSICAL_MINIMUM   (0)     */\
0x75, 0x02,                  /*   (GLOBAL) REPORT_SIZE        0x02 (2) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 2 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x55, 0x00,                  /*   (GLOBAL) UNIT_EXPONENT      0x00 (Unit Value x 10⁰)    */\
0x45, 0x01,                  /*   (GLOBAL) PHYSICAL_MAXIMUM   0x01 (1)     */\
0x34,                        /*   (GLOBAL) PHYSICAL_MINIMUM   (0) <-- Redundant: PHYSICAL_MINIMUM is already 0    */\
0x09, 0x0A,                  /*   (LOCAL)  USAGE              0xFF0C000A     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x05,                  /*   (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x09, 0x0B,                  /*   (LOCAL)  USAGE              0xFF0C000B     */\
0x44,                        /*   (GLOBAL) PHYSICAL_MAXIMUM   (0)     */\
0x34,                        /*   (GLOBAL) PHYSICAL_MINIMUM   (0) <-- Redundant: PHYSICAL_MINIMUM is already 0    */\
0x75, 0x10,                  /*   (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined    */\
0x09, 0x23,                  /*   (LOCAL)  USAGE              0xFF000023     */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=)   */\
0x09, 0x0C,                  /*     (LOCAL)  USAGE              0xFF00000C       */\
0x06, 0x0C, 0xFF,            /*     (GLOBAL) USAGE_PAGE         0xFF0C       */\
0x75, 0x40,                  /*     (GLOBAL) REPORT_SIZE        0x40 (64) Number of bits per field       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 64 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page    */\
0x0A, 0x0E, 0x03,            /*   (LOCAL)  USAGE              0x0020030E Property: Report Interval (DV=Dynamic Value)    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x85, 0x02,                  /*   (GLOBAL) REPORT_ID          0x02 (2)    */\
0x0A, 0x1B, 0x03,            /*   (LOCAL)  USAGE              0x0020031B Property: Report Latency (DV=Dynamic Value)    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Sensor Device Page HIDAccelFeatureReport 01 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:01 RCNT:1
    // Locals:  USAG:0020030E UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  0020030E
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    uint32_t SNS_VendorDefinedPropertyReportInterval;  // Usage 0x0020030E: Property: Report Interval, Value = 0 to 0
} HIDAccelFeatureReport01;


//--------------------------------------------------------------------------------
// Sensor Device Page HIDAccelFeatureReport 02 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x02 (2)
    
    // Field:   2
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:02 RCNT:1
    // Locals:  USAG:0020031B UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  0020031B
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    uint32_t SNS_VendorDefinedPropertyReportLatency;   // Usage 0x0020031B: Property: Report Latency, Value = 0 to 0
} HIDAccelFeatureReport02;


//--------------------------------------------------------------------------------
// Sensor Device Page HIDAccelInputReport 01 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:-32768 LMAX:32767 PMIN:-32768 PMAX:32767 UEXP:-3 UNIT:0 RSIZ:16 RID:01 RCNT:1
    // Locals:  USAG:00200453 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00200453
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    // Collection: Motion:Accelerometer3D
    int16_t  SNS_MotionAccelerometer3DDataFieldAccelerationAxisX; // Usage 0x00200453: Data Field: Acceleration Axis X, Value = -32768 to 32767, Physical = ((Value + 32768) - 32768)
    
    // Field:   2
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:-32768 LMAX:32767 PMIN:-32768 PMAX:32767 UEXP:-3 UNIT:0 RSIZ:16 RID:01 RCNT:1
    // Locals:  USAG:00200454 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00200454
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    int16_t  SNS_MotionAccelerometer3DDataFieldAccelerationAxisY; // Usage 0x00200454: Data Field: Acceleration Axis Y, Value = -32768 to 32767, Physical = ((Value + 32768) - 32768)
    
    // Field:   3
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:-32768 LMAX:32767 PMIN:-32768 PMAX:32767 UEXP:-3 UNIT:0 RSIZ:16 RID:01 RCNT:1
    // Locals:  USAG:00200455 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00200455
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    int16_t  SNS_MotionAccelerometer3DDataFieldAccelerationAxisZ; // Usage 0x00200455: Data Field: Acceleration Axis Z, Value = -32768 to 32767, Physical = ((Value + 32768) - 32768)
    
    // Field:   4
    // Width:   2
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:2 UEXP:-3 UNIT:0 RSIZ:2 RID:01 RCNT:1
    // Locals:  USAG:FF0C0009 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF0C0009
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint8_t  MotionAccelerometer3D0009 : 2;            // Usage 0xFF0C0009: , Value = 0 to 0, Physical = Value / 0
    
    // Field:   5
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:1
    // Locals:  USAG:FF0C000A UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF0C000A
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint8_t  MotionAccelerometer3D000A : 1;            // Usage 0xFF0C000A: , Value = 0 to 0, Physical = Value / 0
    
    // Field:   6
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:5 RID:01 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Only
    // Type:    Array
    // Page 0xFF0C:
    uint8_t  : 5;                                      // Pad
    
    // Field:   7
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:01 RCNT:1
    // Locals:  USAG:FF0C000B UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF0C000B
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint16_t MotionAccelerometer3D000B;                // Usage 0xFF0C000B: , Value = 0 to 0
    
    // Field:   8
    // Width:   64
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:64 RID:01 RCNT:1
    // Locals:  USAG:FF00000C UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF00000C
    // Coll:    Motion:Accelerometer3D
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint64_t VEN_MotionAccelerometer3D000C;            // Usage 0xFF00000C: , Value = 0 to 0
} HIDAccelInputReport01;



//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDClef \
0x06, 0x00, 0xFF,            /* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined  */\
0x09, 0x30,                  /* (LOCAL)  USAGE              0xFF000030   */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0xFF000030: Page=Vendor-defined, Usage=, Type=) */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
0x09, 0x23,                  /*   (LOCAL)  USAGE              0xFF000023     */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=)   */\
0x85, 0x01,                  /*     (GLOBAL) REPORT_ID          0x01 (1)      */\
0x06, 0x14, 0xFF,            /*     (GLOBAL) USAGE_PAGE         0xFF14 Vendor-defined      */\
0x09, 0x01,                  /*     (LOCAL)  USAGE              0xFF140001       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x75, 0x30,                  /*     (GLOBAL) REPORT_SIZE        0x30 (48) Number of bits per field       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 48 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x06, 0x00, 0xFF,            /*     (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined      */\
0x09, 0x23,                  /*     (LOCAL)  USAGE              0xFF000023       */\
0xA1, 0x00,                  /*     (MAIN)   COLLECTION         0x00 Physical (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=)     */\
0x09, 0x0C,                  /*       (LOCAL)  USAGE              0xFF00000C         */\
0x06, 0x0C, 0xFF,            /*       (GLOBAL) USAGE_PAGE         0xFF0C         */\
0x75, 0x40,                  /*       (GLOBAL) REPORT_SIZE        0x40 (64) Number of bits per field         */\
0x81, 0x02,                  /*       (MAIN)   INPUT              0x00000002 (1 field x 64 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap        */\
0xC0,                        /*     (MAIN)   END_COLLECTION     Physical     */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined    */\
0x09, 0x23,                  /*   (LOCAL)  USAGE              0xFF000023     */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=)   */\
0x85, 0x02,                  /*     (GLOBAL) REPORT_ID          0x02 (2)      */\
0x06, 0x14, 0xFF,            /*     (GLOBAL) USAGE_PAGE         0xFF14 Vendor-defined      */\
0x09, 0x02,                  /*     (LOCAL)  USAGE              0xFF140002       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x75, 0x18,                  /*     (GLOBAL) REPORT_SIZE        0x18 (24) Number of bits per field       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 24 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x06, 0x00, 0xFF,            /*     (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined      */\
0x09, 0x23,                  /*     (LOCAL)  USAGE              0xFF000023       */\
0xA1, 0x00,                  /*     (MAIN)   COLLECTION         0x00 Physical (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=)     */\
0x09, 0x0C,                  /*       (LOCAL)  USAGE              0xFF00000C         */\
0x06, 0x0C, 0xFF,            /*       (GLOBAL) USAGE_PAGE         0xFF0C         */\
0x75, 0x40,                  /*       (GLOBAL) REPORT_SIZE        0x40 (64) Number of bits per field         */\
0x81, 0x02,                  /*       (MAIN)   INPUT              0x00000002 (1 field x 64 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap        */\
0xC0,                        /*     (MAIN)   END_COLLECTION     Physical     */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page    */\
0x0A, 0x0E, 0x03,            /*   (LOCAL)  USAGE              0x0020030E Property: Report Interval (DV=Dynamic Value)    */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
0x17, 0x00, 0x00, 0x00, 0x00, /*   (GLOBAL) LOGICAL_MINIMUM    0x00000000 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 17 00000000 with 14   */\
0x27, 0x00, 0x00, 0x00, 0x70, /*   (GLOBAL) LOGICAL_MAXIMUM    0x70000000 (1879048192)     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Sensor Device Page HIDClefFeatureReport 01 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:1879048192 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:01 RCNT:1
    // Locals:  USAG:0020030E UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  0020030E
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    uint32_t SNS_VendorDefinedPropertyReportInterval;  // Usage 0x0020030E: Property: Report Interval, Value = 0 to 1879048192
} HIDClefFeatureReport01;


//--------------------------------------------------------------------------------
// Vendor-defined HIDClefInputReport 01 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   48
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF14 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:48 RID:01 RCNT:1
    // Locals:  USAG:FF140001 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF140001
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF14: Vendor-defined
    uint64_t VEN_VendorDefined0001 : 48;               // Usage 0xFF140001: , Value = 0 to 0
    
    // Field:   2
    // Width:   64
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:64 RID:01 RCNT:1
    // Locals:  USAG:FF00000C UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF00000C
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint64_t VEN_VendorDefined000C;                    // Usage 0xFF00000C: , Value = 0 to 0
} HIDClefInputReport01;


//--------------------------------------------------------------------------------
// Vendor-defined HIDClefInputReport 02 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x02 (2)
    
    // Field:   3
    // Width:   24
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF14 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:24 RID:02 RCNT:1
    // Locals:  USAG:FF140002 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF140002
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF14: Vendor-defined
    uint32_t VEN_VendorDefined0002 : 24;               // Usage 0xFF140002: , Value = 0 to 0
    
    // Field:   4
    // Width:   64
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:64 RID:02 RCNT:1
    // Locals:  USAG:FF00000C UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF00000C
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint64_t VEN_VendorDefined000C;                    // Usage 0xFF00000C: , Value = 0 to 0
} HIDClefInputReport02;


///--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDOrientation \
0x05, 0x20,                  /* (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
0x09, 0x8A,                  /* (LOCAL)  USAGE              0x0020008A Orientation: Device Orientation (CACP=Application or Physical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x0020008A: Page=Sensor Device Page, Usage=Orientation: Device Orientation, Type=CACP) */\
0x06, 0x0C, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF0C     */\
0x19, 0x64,                  /*   (LOCAL)  USAGE_MINIMUM      0xFF0C0064     */\
0x2A, 0x6B, 0x00,            /*   (LOCAL)  USAGE_MAXIMUM      0xFF0C006B     */\
0x15, 0x01,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x01 (1)     */\
0x25, 0x08,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x08 (8)     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
0x81, 0x00,                  /*   (MAIN)   INPUT              0x00000000 (1 field x 8 bits) 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
//  HIDOrientationInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000000: 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:FF0C LMIN:1 LMAX:8 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:FF0C0064 UMAX:FF0C006B DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF0C0064 FF0C0065 FF0C0066 FF0C0067 FF0C0068 FF0C0069 FF0C006A FF0C006B
    // Coll:    Orientation:DeviceOrientation
    // Access:  Read/Write
    // Type:    Array
    // Page 0xFF0C:
    // Collection: Orientation:DeviceOrientation
    uint8_t  OrientationDeviceOrientation;             // Value = 1 to 8
} HIDOrientationInputReport;

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDCameraDescriptor \
0x05, 0x90,                  /*   (GLOBAL) USAGE_PAGE         0x0090 Camera Control Page    */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x0: Page=, Usage=, Type=) <-- Warning: USAGE type should be CA (Application) */\
0x19, 0x20,                  /*   (LOCAL)  USAGE_MINIMUM      0x00900020 Camera Auto Focus (OSC=One Shot Control)    */\
0x29, 0x21,                  /*   (LOCAL)  USAGE_MAXIMUM      0x00900021 Camera Shutter (OSC=One Shot Control)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x95, 0x02,                  /*   (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x75, 0x06,                  /*   (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 6 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Camera Control Page HIDCameraDescriptorInputReport (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    // No REPORT ID byte
    
    // Field:   1
    // Width:   1
    // Count:   2
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0090 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:2
    // Locals:  USAG:0 UMIN:00900020 UMAX:00900021 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00900020 00900021
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0090: Camera Control Page
    uint8_t  CAM_VendorDefinedCameraAutoFocus : 1;     // Usage 0x00900020: Camera Auto Focus, Value = 0 to 1
    uint8_t  CAM_VendorDefinedCameraShutter : 1;       // Usage 0x00900021: Camera Shutter, Value = 0 to 1
    
    // Field:   2
    // Width:   6
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:0090 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:6 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:
    // Access:  Read/Only
    // Type:    Array
    // Page 0x0090: Camera Control Page
    uint8_t  : 6;                                      // Pad
} HIDCameraDescriptorInputReport;

#endif /* IOHIDUnitTestDescriptors_h */

