//
//  IOHIDMultipleInterfaceDescriptors.h
//  IOHIDFamily
//
//  Created by Paul on 8/30/18.
//

#ifndef IOHIDMultipleInterfaceDescriptors_h
#define IOHIDMultipleInterfaceDescriptors_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDKeyboardButtonMultiCollection \
0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
0x09, 0x06,                  /* (LOCAL)  USAGE              0x00010006 Keyboard (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010006: Page=Generic Desktop Page, Usage=Keyboard, Type=CA) */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
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
0x05, 0x0B,                  /* (GLOBAL) USAGE_PAGE         0x000B Telephony Device Page  */\
0x09, 0x01,                  /* (LOCAL)  USAGE              0x000B0001 Phone (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000B0001: Page=Telephony Device Page, Usage=Phone, Type=CA) */\
0x85, 0x02,                  /*   (GLOBAL) REPORT_ID          0x02 (2)    */\
0x05, 0x0C,                  /*   (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page    */\
0x09, 0x21,                  /*   (LOCAL)  USAGE              0x000C0021 +100 (OSC=One Shot Control)    */\
0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Keyboard/Keypad Page HIDKeyboardButtonMultiCollectionInputReport 01 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)

    // Field:   1
    // Width:   1
    // Count:   8
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:8
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
    // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:01 RCNT:1
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
    // Globals: PAGE:0007 LMIN:0 LMAX:255 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:01 RCNT:5
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
    // Globals: PAGE:00FF LMIN:0 LMAX:255 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:01 RCNT:1
    // Locals:  USAG:00FF0003 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00FF0003
    // Coll:    Keyboard
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x00FF: Reserved
    uint8_t  RES_Keyboard0003;                         // Usage 0x00FF0003: , Value = 0 to 255
} HIDKeyboardButtonMultiCollectionInputReport01;


//--------------------------------------------------------------------------------
// LED Indicator Page HIDKeyboardButtonMultiCollectionOutputReport 01 (Device <-- Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)

    // Field:   1
    // Width:   1
    // Count:   5
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0008 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:5
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
    // Globals: PAGE:0008 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:3 RID:01 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Keyboard
    // Access:  Read/Only
    // Type:    Array
    // Page 0x0008: LED Indicator Page
    uint8_t  : 3;                                      // Pad
} HIDKeyboardButtonMultiCollectionOutputReport01;

//--------------------------------------------------------------------------------
// Consumer Device Page HIDKeyboardButtonMultiCollectionInputReport 02 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x02 (2)

    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:02 RCNT:1
    // Locals:  USAG:000C0021 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000C0021
    // Coll:    Phone
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000C: Consumer Device Page
    // Collection: Phone
    uint8_t  CD_PhonePlus100;                          // Usage 0x000C0021: +100, Value = 0 to 1
} HIDKeyboardButtonMultiCollectionInputReport02;


//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAppleVendorMultiCollectionHeader \
0x06, 0x00, 0xFF,            /* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined (AppleVendor) */\
0x09, 0x1A,                  /* (LOCAL)  USAGE              0xFF00001A Multiple Interfaces (CA=Application Collection)   */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0xFF00001A: Page=Vendor-defined, Usage=, Type=) */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


#endif /* IOHIDMultipleInterfaceDescriptors_h */
