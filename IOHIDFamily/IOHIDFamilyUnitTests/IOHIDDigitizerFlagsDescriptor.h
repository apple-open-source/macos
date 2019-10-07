//
//  IOHIDGyroDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDDigitizerFlags_h
#define IOHIDDigitizerFlags_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDDigitizerFlags \
0x05, 0x0D,                  /* (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page  */\
0x09, 0x05,                  /* (LOCAL)  USAGE              0x000D0005 Touch Pad (CA=Application Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000D0005: Page=Digitizer Device Page, Usage=Touch Pad, Type=CA) */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page <-- Redundant: USAGE_PAGE is already 0x000D   */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
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
0x09, 0x32,                  /*     (LOCAL)  USAGE              0x000D0032 In Range (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x34,                  /*     (LOCAL)  USAGE              0x000D0034 Untouch (OSC=One Shot Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x05,                  /*     (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767) <-- Redundant: LOGICAL_MAXIMUM is already 32767      */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap  <-- Error: REPORT_SIZE (8) is too small for LOGICAL_MAXIMUM (32767) which needs 15 bits.     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x32,                  /*     (LOCAL)  USAGE              0x000D0032 In Range (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x34,                  /*     (LOCAL)  USAGE              0x000D0034 Untouch (OSC=One Shot Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x05,                  /*     (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767) <-- Redundant: LOGICAL_MAXIMUM is already 32767      */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap  <-- Error: REPORT_SIZE (8) is too small for LOGICAL_MAXIMUM (32767) which needs 15 bits.     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x32,                  /*     (LOCAL)  USAGE              0x000D0032 In Range (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x34,                  /*     (LOCAL)  USAGE              0x000D0034 Untouch (OSC=One Shot Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x05,                  /*     (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767) <-- Redundant: LOGICAL_MAXIMUM is already 32767      */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap  <-- Error: REPORT_SIZE (8) is too small for LOGICAL_MAXIMUM (32767) which needs 15 bits.     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x32,                  /*     (LOCAL)  USAGE              0x000D0032 In Range (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x34,                  /*     (LOCAL)  USAGE              0x000D0034 Untouch (OSC=One Shot Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x05,                  /*     (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767) <-- Redundant: LOGICAL_MAXIMUM is already 32767      */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
0xA1, 0x02,                  /*   (MAIN)   COLLECTION         0x02 Logical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL)   */\
0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
0x75, 0x08,                  /*     (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap  <-- Error: REPORT_SIZE (8) is too small for LOGICAL_MAXIMUM (32767) which needs 15 bits.     */\
0x09, 0x33,                  /*     (LOCAL)  USAGE              0x000D0033 Touch (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x32,                  /*     (LOCAL)  USAGE              0x000D0032 In Range (MC=Momentary Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x34,                  /*     (LOCAL)  USAGE              0x000D0034 Untouch (OSC=One Shot Control)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1) <-- Redundant: LOGICAL_MAXIMUM is already 1      */\
0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x75, 0x05,                  /*     (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x01,                  /*     (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull      */\
0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)       */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
0x15, 0x00,                  /*     (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14     */\
0x26, 0xFF, 0x7F,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767) <-- Redundant: LOGICAL_MAXIMUM is already 32767      */\
0x75, 0x10,                  /*     (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16      */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1      */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Logical   */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDDigitizerFlagsInputReport (Device --> Host)
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
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0032 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0032
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerInRange : 1;            // Usage 0x000D0032: In Range, Value = 0 to 1
    
    // Field:   4
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0034 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0034
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerUntouch : 1;            // Usage 0x000D0034: Untouch, Value = 0 to 1
    
    // Field:   5
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 5;                                      // Pad
    
    // Field:   6
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX;                       // Usage 0x00010030: X, Value = 0 to 32767
    
    // Field:   7
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY;                       // Usage 0x00010031: Y, Value = 0 to 32767
    
    // Field:   8
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0038
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTransducerIndex_1;      // Usage 0x000D0038: Transducer Index, Value = 0 to 32767
    
    // Field:   9
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
    
    // Field:   10
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0032 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0032
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerInRange_1 : 1;          // Usage 0x000D0032: In Range, Value = 0 to 1
    
    // Field:   11
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0034 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0034
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerUntouch_1 : 1;          // Usage 0x000D0034: Untouch, Value = 0 to 1
    
    // Field:   12
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 5;                                      // Pad
    
    // Field:   13
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX_1;                     // Usage 0x00010030: X, Value = 0 to 32767
    
    // Field:   14
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY_1;                     // Usage 0x00010031: Y, Value = 0 to 32767
    
    // Field:   15
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0038
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTransducerIndex_2;      // Usage 0x000D0038: Transducer Index, Value = 0 to 32767
    
    // Field:   16
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
    uint8_t  DIG_TouchPadFingerTouch_2 : 1;            // Usage 0x000D0033: Touch, Value = 0 to 1
    
    // Field:   17
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0032 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0032
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerInRange_2 : 1;          // Usage 0x000D0032: In Range, Value = 0 to 1
    
    // Field:   18
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0034 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0034
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerUntouch_2 : 1;          // Usage 0x000D0034: Untouch, Value = 0 to 1
    
    // Field:   19
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 5;                                      // Pad
    
    // Field:   20
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX_2;                     // Usage 0x00010030: X, Value = 0 to 32767
    
    // Field:   21
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY_2;                     // Usage 0x00010031: Y, Value = 0 to 32767
    
    // Field:   22
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0038
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTransducerIndex_3;      // Usage 0x000D0038: Transducer Index, Value = 0 to 32767
    
    // Field:   23
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
    uint8_t  DIG_TouchPadFingerTouch_3 : 1;            // Usage 0x000D0033: Touch, Value = 0 to 1
    
    // Field:   24
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0032 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0032
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerInRange_3 : 1;          // Usage 0x000D0032: In Range, Value = 0 to 1
    
    // Field:   25
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0034 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0034
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerUntouch_3 : 1;          // Usage 0x000D0034: Untouch, Value = 0 to 1
    
    // Field:   26
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 5;                                      // Pad
    
    // Field:   27
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX_3;                     // Usage 0x00010030: X, Value = 0 to 32767
    
    // Field:   28
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY_3;                     // Usage 0x00010031: Y, Value = 0 to 32767
    
    // Field:   29
    // Width:   8
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:0 RCNT:1
    // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0038
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerTransducerIndex_4;      // Usage 0x000D0038: Transducer Index, Value = 0 to 32767
    
    // Field:   30
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
    uint8_t  DIG_TouchPadFingerTouch_4 : 1;            // Usage 0x000D0033: Touch, Value = 0 to 1
    
    // Field:   31
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0032 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0032
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerInRange_4 : 1;          // Usage 0x000D0032: In Range, Value = 0 to 1
    
    // Field:   32
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:1
    // Locals:  USAG:000D0034 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000D0034
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x000D: Digitizer Device Page
    uint8_t  DIG_TouchPadFingerUntouch_4 : 1;          // Usage 0x000D0034: Untouch, Value = 0 to 1
    
    // Field:   33
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:5 RID:0 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    TouchPad Finger
    // Access:  Read/Only
    // Type:    Array
    // Page 0x000D: Digitizer Device Page
    uint8_t  : 5;                                      // Pad
    
    // Field:   34
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010030
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerX_4;                     // Usage 0x00010030: X, Value = 0 to 32767
    
    // Field:   35
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0001 LMIN:0 LMAX:32767 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:0 RCNT:1
    // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00010031
    // Coll:    TouchPad Finger
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0001: Generic Desktop Page
    uint16_t GD_TouchPadFingerY_4;                     // Usage 0x00010031: Y, Value = 0 to 32767
} HIDDigitizerFlagsInputReport;


#endif /* IOHIDDigitizerFlags_h */
