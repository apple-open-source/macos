//
//  IOHIDCarPlaySelectButton.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDCarPlaySelectButton_h
#define IOHIDCarPlaySelectButton_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

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

#endif /* IOHIDCarPlaySelectButton_h */
