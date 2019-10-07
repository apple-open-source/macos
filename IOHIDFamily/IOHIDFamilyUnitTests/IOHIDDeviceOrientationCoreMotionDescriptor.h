//
//  IOHIDDeviceOrientationCoreMotionDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDDeviceOrientationCoreMotionDescriptor_h
#define IOHIDDeviceOrientationCoreMotionDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDOrientationCoreMotion \
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
} HIDOrientationCoreMotionInputReport;

#endif /* IOHIDDeviceOrientationCoreMotionDescriptor_h */
