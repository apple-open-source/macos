//
//  IOHIDDisplayOrientationTiltDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDDisplayOrientationTiltDescriptor_h
#define IOHIDDisplayOrientationTiltDescriptor_h

///--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDDisplayOrientationTilt \
0x05, 0x20,                  /* (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
0x09, 0x8A,                  /* (LOCAL)  USAGE              0x0020008A Orientation: Device Orientation (CACP=Application or Physical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x0020008A: Page=Sensor Device Page, Usage=Orientation: Device Orientation, Type=CACP) */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
0x46, 0x68, 0x01,            /*   (GLOBAL) PHYSICAL_MAXIMUM   0x0168 (360)     */\
0x34,                        /*   (GLOBAL) PHYSICAL_MINIMUM   (0) <-- Redundant: PHYSICAL_MINIMUM is already 0    */\
0x26, 0x68, 0x01,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x0168 (360)     */\
0x14,                        /*   (GLOBAL) LOGICAL_MINIMUM    (0) <-- Redundant: LOGICAL_MINIMUM is already 0    */\
0x65, 0x14,                  /*   (GLOBAL) UNIT               0x00000014 Rotation in degrees [1° units] (4=System=English Rotation, 1=Rotation=Degrees)    */\
0x0A, 0x7F, 0x04,            /*   (LOCAL)  USAGE              0x0020047F Data Field: Tilt X Axis (SV=Static Value)    */\
0x0A, 0x80, 0x04,            /*   (LOCAL)  USAGE              0x00200480 Data Field: Tilt Y Axis (SV=Static Value)    */\
0x0A, 0x81, 0x04,            /*   (LOCAL)  USAGE              0x00200481 Data Field: Tilt Z Axis (SV=Static Value)    */\
0x95, 0x03,                  /*   (GLOBAL) REPORT_COUNT       0x03 (3) Number of fields     */\
0x75, 0x09,                  /*   (GLOBAL) REPORT_SIZE        0x09 (9) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (3 fields x 9 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x05,                  /*   (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field     */\
0x81, 0x05,                  /*   (MAIN)   INPUT              0x00000005 (1 field x 5 bits) 1=Constant 0=Array 1=Relative 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Sensor Device Page HIDDisplayOrientationTiltInputReport 01 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   9
    // Count:   3
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:360 PMIN:0 PMAX:360 UEXP:0 UNIT:00000014 RSIZ:9 RID:01 RCNT:3
    // Locals:  USAG:00200481 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  0020047F 00200480 00200481
    // Coll:    Orientation:DeviceOrientation
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    // Collection: Orientation:DeviceOrientation
    uint16_t SNS_OrientationDeviceOrientationDataFieldTiltXAxis : 9; // Usage 0x0020047F: Data Field: Tilt X Axis, Value = 0 to 360, Physical = Value in degrees
    uint16_t SNS_OrientationDeviceOrientationDataFieldTiltYAxis : 9; // Usage 0x00200480: Data Field: Tilt Y Axis, Value = 0 to 360, Physical = Value in degrees
    uint16_t SNS_OrientationDeviceOrientationDataFieldTiltZAxis : 9; // Usage 0x00200481: Data Field: Tilt Z Axis, Value = 0 to 360, Physical = Value in degrees
    
    // Field:   2
    // Width:   5
    // Count:   1
    // Flags:   00000005: 1=Constant 0=Array 1=Relative 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:0020 LMIN:0 LMAX:360 PMIN:0 PMAX:360 UEXP:0 UNIT:00000014 RSIZ:5 RID:01 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Orientation:DeviceOrientation
    // Access:  Read/Only
    // Type:    Array
    // Page 0x0020: Sensor Device Page
    uint8_t  : 5;                                      // Pad
} HIDDisplayOrientationTiltInputReport01;

#endif /* IOHIDDisplayOrientationTiltDescriptor_h */
