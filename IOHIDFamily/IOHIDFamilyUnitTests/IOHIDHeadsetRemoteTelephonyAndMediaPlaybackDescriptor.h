//
//  IOHIDHeadsetRemoteTelephonyAndMediaPlaybackInputReportDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDHeadsetRemoteTelephonyAndMediaPlaybackInputReportDescriptor_h
#define IOHIDHeadsetRemoteTelephonyAndMediaPlaybackInputReportDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDHeadsetRemoteTelephonyAndMediaPlayback \
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
0x0A, 0x5B, 0x02,            /*   (LOCAL)  USAGE              0x000C025B AC Promote (Sel=Selector)    */\
0x0A, 0x5C, 0x02,            /*   (LOCAL)  USAGE              0x000C025C AC Demote (Sel=Selector)    */\
0x0A, 0x62, 0x02,            /*   (LOCAL)  USAGE              0x000C0262 AC Add to Cart (Sel=Selector)    */\
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
// Consumer Device Page HIDHeadsetRemoteTelephonyAndMediaPlaybackInputReport (Device --> Host)
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
    // Usages:  000C00B5 000C00B6 000C00E2 000C00B9 000C00BC 000C025B 000C025C 000C0262 000C00E9 000C00EA
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
    uint8_t  CD_ConsumerControlAcPromote : 1;          // Usage 0x000C025B: AC Promote, Value = 0 to 1
    uint8_t  CD_ConsumerControlAcDemote : 1;           // Usage 0x000C025C: AC Demote, Value = 0 to 1
    uint8_t  CD_ConsumerControlAcAddToCart : 1;        // Usage 0x000C0262: AC Add to Cart, Value = 0 to 1
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
} HIDHeadsetRemoteTelephonyAndMediaPlaybackInputReport;

#endif /* IOHIDHeadsetRemoteTelephonyAndMediaPlaybackInputReportDescriptor_h */
