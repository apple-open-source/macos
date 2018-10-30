//
//  IOHIDGyroDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDGyroDescriptor_h
#define IOHIDGyroDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDCompass \
0x05, 0x20,                  /* (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page  */\
0x09, 0x8B,                  /* (LOCAL)  USAGE              0x0020008B Orientation: Compass (CACP=Application or Physical Collection)  */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x0020008B: Page=Sensor Device Page, Usage=Orientation: Compass, Type=CACP) */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
0x46, 0xFF, 0x7F,            /*   (GLOBAL) PHYSICAL_MAXIMUM   0x7FFF (32767)     */\
0x36, 0x00, 0x80,            /*   (GLOBAL) PHYSICAL_MINIMUM   0x8000 (-32768)     */\
0x26, 0xFF, 0x7F,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x7FFF (32767)     */\
0x16, 0x00, 0x80,            /*   (GLOBAL) LOGICAL_MINIMUM    0x8000 (-32768)     */\
0x55, 0x0D,                  /*   (GLOBAL) UNIT_EXPONENT      0x0D (Unit Value x 10⁻³)    */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page <-- Redundant: USAGE_PAGE is already 0x0020   */\
0x0A, 0x85, 0x04,            /*   (LOCAL)  USAGE              0x00200485 Data Field: Magnetic Flux X Axis (SV=Static Value)    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page <-- Redundant: USAGE_PAGE is already 0x0020   */\
0x0A, 0x86, 0x04,            /*   (LOCAL)  USAGE              0x00200486 Data Field: Magnetic Flux Y Axis (SV=Static Value)    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page <-- Redundant: USAGE_PAGE is already 0x0020   */\
0x0A, 0x87, 0x04,            /*   (LOCAL)  USAGE              0x00200487 Data Field: Magnetic Flux Z Axis (SV=Static Value)    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x06, 0x0C, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF0C     */\
0x09, 0x09,                  /*   (LOCAL)  USAGE              0xFF0C0009     */\
0x24,                        /*   (GLOBAL) LOGICAL_MAXIMUM    (0)     */\
0x14,                        /*   (GLOBAL) LOGICAL_MINIMUM    (0)     */\
0x45, 0x02,                  /*   (GLOBAL) PHYSICAL_MAXIMUM   0x02 (2)     */\
0x35, 0x00,                  /*   (GLOBAL) PHYSICAL_MINIMUM   0x00 (0)  <-- Info: Consider replacing 35 00 with 34   */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x02,                  /*   (GLOBAL) REPORT_SIZE        0x02 (2) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 2 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x55, 0x00,                  /*   (GLOBAL) UNIT_EXPONENT      0x00 (Unit Value x 10⁰)    */\
0x45, 0x01,                  /*   (GLOBAL) PHYSICAL_MAXIMUM   0x01 (1)     */\
0x35, 0x00,                  /*   (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34   */\
0x06, 0x0C, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF0C  <-- Redundant: USAGE_PAGE is already 0xFF0C   */\
0x09, 0x0A,                  /*   (LOCAL)  USAGE              0xFF0C000A     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x05,                  /*   (GLOBAL) REPORT_SIZE        0x05 (5) Number of bits per field     */\
0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 5 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x06, 0x0C, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF0C  <-- Redundant: USAGE_PAGE is already 0xFF0C   */\
0x09, 0x0B,                  /*   (LOCAL)  USAGE              0xFF0C000B     */\
0x45, 0x00,                  /*   (GLOBAL) PHYSICAL_MAXIMUM   0x00 (0)  <-- Info: Consider replacing 45 00 with 44   */\
0x35, 0x00,                  /*   (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34   */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page    */\
0x0A, 0x0E, 0x03,            /*   (LOCAL)  USAGE              0x0020030E Property: Report Interval (DV=Dynamic Value)    */\
0x17, 0x00, 0x00, 0x00, 0x00, /*   (GLOBAL) LOGICAL_MINIMUM    0x00000000 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 17 00000000 with 14   */\
0x27, 0x00, 0x00, 0x00, 0x70, /*   (GLOBAL) LOGICAL_MAXIMUM    0x70000000 (1879048192)     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Sensor Device Page HIDCompassFeatureReport 01 (Device <-> Host)
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
    // Coll:    Orientation:Compass
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    // Collection: Orientation:Compass
    uint32_t SNS_OrientationCompassPropertyReportInterval; // Usage 0x0020030E: Property: Report Interval, Value = 0 to 1879048192
} HIDCompassFeatureReport01;


//--------------------------------------------------------------------------------
// Sensor Device Page HIDCompassInputReport 01 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:-32768 LMAX:32767 PMIN:-32768 PMAX:32767 UEXP:-3 UNIT:0 RSIZ:32 RID:01 RCNT:1
    // Locals:  USAG:00200485 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00200485
    // Coll:    Orientation:Compass
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    // Collection: Orientation:Compass
    int32_t  SNS_OrientationCompassDataFieldMagneticFluxXAxis; // Usage 0x00200485: Data Field: Magnetic Flux X Axis, Value = -32768 to 32767, Physical = ((Value + 32768) - 32768)
    
    // Field:   2
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:-32768 LMAX:32767 PMIN:-32768 PMAX:32767 UEXP:-3 UNIT:0 RSIZ:32 RID:01 RCNT:1
    // Locals:  USAG:00200486 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00200486
    // Coll:    Orientation:Compass
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    int32_t  SNS_OrientationCompassDataFieldMagneticFluxYAxis; // Usage 0x00200486: Data Field: Magnetic Flux Y Axis, Value = -32768 to 32767, Physical = ((Value + 32768) - 32768)
    
    // Field:   3
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:-32768 LMAX:32767 PMIN:-32768 PMAX:32767 UEXP:-3 UNIT:0 RSIZ:32 RID:01 RCNT:1
    // Locals:  USAG:00200487 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  00200487
    // Coll:    Orientation:Compass
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    int32_t  SNS_OrientationCompassDataFieldMagneticFluxZAxis; // Usage 0x00200487: Data Field: Magnetic Flux Z Axis, Value = -32768 to 32767, Physical = ((Value + 32768) - 32768)
    
    // Field:   4
    // Width:   2
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:2 UEXP:-3 UNIT:0 RSIZ:2 RID:01 RCNT:1
    // Locals:  USAG:FF0C0009 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF0C0009
    // Coll:    Orientation:Compass
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint8_t  OrientationCompass0009 : 2;               // Usage 0xFF0C0009: , Value = 0 to 0, Physical = Value / 0
    
    // Field:   5
    // Width:   1
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:1
    // Locals:  USAG:FF0C000A UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF0C000A
    // Coll:    Orientation:Compass
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint8_t  OrientationCompass000A : 1;               // Usage 0xFF0C000A: , Value = 0 to 0, Physical = Value / 0
    
    // Field:   6
    // Width:   5
    // Count:   1
    // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:1 UEXP:0 UNIT:0 RSIZ:5 RID:01 RCNT:1
    // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:
    // Coll:    Orientation:Compass
    // Access:  Read/Only
    // Type:    Array
    // Page 0xFF0C:
    uint8_t  : 5;                                      // Pad
    
    // Field:   7
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF0C LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:01 RCNT:1
    // Locals:  USAG:FF0C000B UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF0C000B
    // Coll:    Orientation:Compass
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF0C:
    uint32_t OrientationCompass000B;                   // Usage 0xFF0C000B: , Value = 0 to 0
} HIDCompassInputReport01;



#endif /* IOHIDGyroDescriptor_h */
