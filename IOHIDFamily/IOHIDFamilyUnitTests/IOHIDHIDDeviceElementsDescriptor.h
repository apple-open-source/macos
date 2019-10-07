//
//  IOHIDHIDDeviceElementsDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 8/13/18.
//

#ifndef IOHIDHIDDeviceElementsDescriptor_h
#define IOHIDHIDDeviceElementsDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDDeviceElements \
0x06, 0x00, 0xFF,            /* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined  */\
0x09, 0x80,                  /* (LOCAL)  USAGE              0xFF000080   */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0xFF000080: Page=Vendor-defined, Usage=, Type=) */\
0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
0x19, 0x10,                  /*   (LOCAL)  USAGE_MINIMUM      0xFF000010     */\
0x29, 0x17,                  /*   (LOCAL)  USAGE_MAXIMUM      0xFF000017     */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
0xB1, 0x00,                  /*   (MAIN)   FEATURE            0x00000000 (1 field x 8 bits) 0=Data 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x85, 0x02,                  /*   (GLOBAL) REPORT_ID          0x02 (2)    */\
0x19, 0x20,                  /*   (LOCAL)  USAGE_MINIMUM      0xFF000020     */\
0x29, 0x27,                  /*   (LOCAL)  USAGE_MAXIMUM      0xFF000027     */\
0x91, 0x00,                  /*   (MAIN)   OUTPUT             0x00000000 (1 field x 8 bits) 0=Data 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x85, 0x03,                  /*   (GLOBAL) REPORT_ID          0x03 (3)    */\
0x19, 0x30,                  /*   (LOCAL)  USAGE_MINIMUM      0xFF000030     */\
0x29, 0x37,                  /*   (LOCAL)  USAGE_MAXIMUM      0xFF000037     */\
0x81, 0x00,                  /*   (MAIN)   INPUT              0x00000000 (1 field x 8 bits) 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
0x85, 0x04,                  /*   (GLOBAL) REPORT_ID          0x04 (4)    */\
0x95, 0x08,                  /*   (GLOBAL) REPORT_COUNT       0x08 (8) Number of fields     */\
0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
0x19, 0x40,                  /*   (LOCAL)  USAGE_MINIMUM      0xFF000040     */\
0x29, 0x47,                  /*   (LOCAL)  USAGE_MAXIMUM      0xFF000047     */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (8 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x85, 0x05,                  /*   (GLOBAL) REPORT_ID          0x05 (5)    */\
0x19, 0x50,                  /*   (LOCAL)  USAGE_MINIMUM      0xFF000050     */\
0x29, 0x57,                  /*   (LOCAL)  USAGE_MAXIMUM      0xFF000057     */\
0x91, 0x02,                  /*   (MAIN)   OUTPUT             0x00000002 (8 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0x85, 0x06,                  /*   (GLOBAL) REPORT_ID          0x06 (6)    */\
0x19, 0x60,                  /*   (LOCAL)  USAGE_MINIMUM      0xFF000060     */\
0x29, 0x67,                  /*   (LOCAL)  USAGE_MAXIMUM      0xFF000067     */\
0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (8 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Vendor-defined HIDDeviceElementsFeatureReport 01 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)
    
    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000000: 0=Data 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:01 RCNT:1
    // Locals:  USAG:0 UMIN:FF000010 UMAX:FF000017 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000010 FF000011 FF000012 FF000013 FF000014 FF000015 FF000016 FF000017
    // Coll:
    // Access:  Read/Write
    // Type:    Array
    // Page 0xFF00: Vendor-defined
    uint8_t  VEN_VendorDefined;                        // Value = 0 to 0
} HIDDeviceElementsFeatureReport01;


//--------------------------------------------------------------------------------
// Vendor-defined HIDDeviceElementsFeatureReport 04 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x04 (4)
    
    // Field:   2
    // Width:   1
    // Count:   8
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:04 RCNT:8
    // Locals:  USAG:0 UMIN:FF000040 UMAX:FF000047 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000040 FF000041 FF000042 FF000043 FF000044 FF000045 FF000046 FF000047
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF00: Vendor-defined
    uint8_t  VEN_VendorDefined0040 : 1;                // Usage 0xFF000040: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0041 : 1;                // Usage 0xFF000041: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0042 : 1;                // Usage 0xFF000042: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0043 : 1;                // Usage 0xFF000043: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0044 : 1;                // Usage 0xFF000044: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0045 : 1;                // Usage 0xFF000045: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0046 : 1;                // Usage 0xFF000046: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0047 : 1;                // Usage 0xFF000047: , Value = 0 to 0
} HIDDeviceElementsFeatureReport04;


//--------------------------------------------------------------------------------
// Vendor-defined HIDDeviceElementsInputReport 03 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x03 (3)
    
    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000000: 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:03 RCNT:1
    // Locals:  USAG:0 UMIN:FF000030 UMAX:FF000037 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000030 FF000031 FF000032 FF000033 FF000034 FF000035 FF000036 FF000037
    // Coll:
    // Access:  Read/Write
    // Type:    Array
    // Page 0xFF00: Vendor-defined
    uint8_t  VEN_VendorDefined;                        // Value = 0 to 0
} HIDDeviceElementsInputReport03;


//--------------------------------------------------------------------------------
// Vendor-defined HIDDeviceElementsInputReport 06 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x06 (6)
    
    // Field:   2
    // Width:   1
    // Count:   8
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:06 RCNT:8
    // Locals:  USAG:0 UMIN:FF000060 UMAX:FF000067 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000060 FF000061 FF000062 FF000063 FF000064 FF000065 FF000066 FF000067
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF00: Vendor-defined
    uint8_t  VEN_VendorDefined0060 : 1;                // Usage 0xFF000060: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0061 : 1;                // Usage 0xFF000061: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0062 : 1;                // Usage 0xFF000062: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0063 : 1;                // Usage 0xFF000063: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0064 : 1;                // Usage 0xFF000064: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0065 : 1;                // Usage 0xFF000065: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0066 : 1;                // Usage 0xFF000066: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0067 : 1;                // Usage 0xFF000067: , Value = 0 to 0
} HIDDeviceElementsInputReport06;


//--------------------------------------------------------------------------------
// Vendor-defined HIDDeviceElementsOutputReport 02 (Device <-- Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x02 (2)
    
    // Field:   1
    // Width:   8
    // Count:   1
    // Flags:   00000000: 0=Data 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:02 RCNT:1
    // Locals:  USAG:0 UMIN:FF000020 UMAX:FF000027 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000020 FF000021 FF000022 FF000023 FF000024 FF000025 FF000026 FF000027
    // Coll:
    // Access:  Read/Write
    // Type:    Array
    // Page 0xFF00: Vendor-defined
    uint8_t  VEN_VendorDefined;                        // Value = 0 to 0
} HIDDeviceElementsOutputReport02;


//--------------------------------------------------------------------------------
// Vendor-defined HIDDeviceElementsOutputReport 05 (Device <-- Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x05 (5)
    
    // Field:   2
    // Width:   1
    // Count:   8
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF00 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:05 RCNT:8
    // Locals:  USAG:0 UMIN:FF000050 UMAX:FF000057 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF000050 FF000051 FF000052 FF000053 FF000054 FF000055 FF000056 FF000057
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF00: Vendor-defined
    uint8_t  VEN_VendorDefined0050 : 1;                // Usage 0xFF000050: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0051 : 1;                // Usage 0xFF000051: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0052 : 1;                // Usage 0xFF000052: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0053 : 1;                // Usage 0xFF000053: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0054 : 1;                // Usage 0xFF000054: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0055 : 1;                // Usage 0xFF000055: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0056 : 1;                // Usage 0xFF000056: , Value = 0 to 0
    uint8_t  VEN_VendorDefined0057 : 1;                // Usage 0xFF000057: , Value = 0 to 0
} HIDDeviceElementsOutputReport05;

#endif /* IOHIDHIDDeviceElementsDescriptor_h */
