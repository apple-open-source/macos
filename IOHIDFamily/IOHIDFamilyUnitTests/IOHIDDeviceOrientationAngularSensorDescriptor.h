//
//  IOHIDDeviceOrientationAngularSensorDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDDeviceOrientationAngularSensorDescriptor_h
#define IOHIDDeviceOrientationAngularSensorDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDAngularSensor \
0x05, 0x20,                  /* (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
0x09, 0x8A,                  /* (LOCAL)  USAGE              0x0020008A Orientation: Device Orientation (CACP=Application or Physical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x0020008A: Page=Sensor Device Page, Usage=Orientation: Device Orientation, Type=CACP) */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
0x46, 0x50, 0x46,            /*   (GLOBAL) PHYSICAL_MAXIMUM   0x4650 (18000)     */\
0x34,                        /*   (GLOBAL) PHYSICAL_MINIMUM   (0) <-- Redundant: PHYSICAL_MINIMUM is already 0    */\
0x26, 0x50, 0x46,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x4650 (18000)     */\
0x14,                        /*   (GLOBAL) LOGICAL_MINIMUM    (0) <-- Redundant: LOGICAL_MINIMUM is already 0    */\
0x55, 0x0E,                  /*   (GLOBAL) UNIT_EXPONENT      0x0E (Unit Value x 10⁻²)    */\
0x65, 0x14,                  /*   (GLOBAL) UNIT               0x00000014 Rotation in degrees [1° units] (4=System=English Rotation, 1=Rotation=Degrees)    */\
0x0A, 0x80, 0x04,            /*   (LOCAL)  USAGE              0x00200480 Data Field: Tilt Y Axis (SV=Static Value)    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x10,                  /*   (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x0A, 0x0E, 0x03,            /*   (LOCAL)  USAGE              0x0020030E Property: Report Interval (DV=Dynamic Value)    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Sensor Device Page HIDAngularSensorFeatureReport 01 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:18000 PMIN:0 PMAX:18000 UEXP:-2 UNIT:00000014 RSIZ:32 RID:01 RCNT:1
    // Locals:  USAG:0020030E UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  0020030E
    // Coll:    Orientation:DeviceOrientation
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    // Collection: Orientation:DeviceOrientation
    uint32_t SNS_OrientationDeviceOrientationPropertyReportInterval; // Usage 0x0020030E: Property: Report Interval, Value = 0 to 18000, Physical = Value in 10⁻² degrees units
} HIDAngularSensorFeatureReport01;


//--------------------------------------------------------------------------------
// Sensor Device Page HIDAngularSensorInputReport 01 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   16
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:18000 PMIN:0 PMAX:18000 UEXP:-2 UNIT:00000014 RSIZ:16 RID:01 RCNT:1
    // Locals:  USAG:00200480 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00200480
    // Coll:    Orientation:DeviceOrientation
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    // Collection: Orientation:DeviceOrientation
    uint16_t SNS_OrientationDeviceOrientationDataFieldTiltYAxis; // Usage 0x00200480: Data Field: Tilt Y Axis, Value = 0 to 18000, Physical = Value in 10⁻² degrees units
} HIDAngularSensorInputReport01;

#endif /* IOHIDDeviceOrientationAngularSensorDescriptor_h */
