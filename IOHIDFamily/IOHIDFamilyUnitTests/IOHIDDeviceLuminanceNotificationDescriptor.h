//
//  IOHIDDeviceLuminanceNotificationDescriptor.h
//  IOHIDFamily
//
//  Created by yg on 1/27/19.
//

#ifndef IOHIDDeviceLuminanceNotificationDescriptor_h
#define IOHIDDeviceLuminanceNotificationDescriptor_h

//--------------------------------------------------------------------------------
// Decoded Application Collection
//--------------------------------------------------------------------------------

#define HIDLuminanceNotification \
0x06, 0x00, 0xFF,            /* (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined  */\
0x09, 0x37,                  /* (LOCAL)  USAGE              0xFF000037   */\
0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0xFF000037: Page=Vendor-defined, Usage=, Type=) */\
0x85, 0x10,                  /*   (GLOBAL) REPORT_ID          0x10 (16)    */\
0x06, 0x00, 0xFF,            /*   (GLOBAL) USAGE_PAGE         0xFF00 Vendor-defined <-- Redundant: USAGE_PAGE is already 0xFF00   */\
0x09, 0x23,                  /*   (LOCAL)  USAGE              0xFF000023     */\
0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0xFF000023: Page=Vendor-defined, Usage=, Type=)   */\
0x06, 0x15, 0xFF,            /*     (GLOBAL) USAGE_PAGE         0xFF15 Vendor-defined      */\
0x09, 0x03,                  /*     (LOCAL)  USAGE              0xFF150003       */\
0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
0x75, 0x20,                  /*     (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field       */\
0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
0x05, 0x20,                  /*   (GLOBAL) USAGE_PAGE         0x0020 Sensor Device Page    */\
0x0A, 0x0E, 0x03,            /*   (LOCAL)  USAGE              0x0020030E Property: Report Interval (DV=Dynamic Value)    */\
0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1    */\
0x75, 0x20,                  /*   (GLOBAL) REPORT_SIZE        0x20 (32) Number of bits per field <-- Redundant: REPORT_SIZE is already 32    */\
0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 32 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
0xC0,                        /* (MAIN)   END_COLLECTION     Application */\


//--------------------------------------------------------------------------------
// Sensor Device Page HIDLuminanceNotificationFeatureReport 10 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x10 (16)
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:0020 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:10 RCNT:1
    // Locals:  USAG:0020030E UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  0020030E
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0x0020: Sensor Device Page
    uint32_t SNS_VendorDefinedPropertyReportInterval;  // Usage 0x0020030E: Property: Report Interval, Value = 0 to 0
} HIDLuminanceNotificationFeatureReport10;


//--------------------------------------------------------------------------------
// Vendor-defined HIDLuminanceNotificationInputReport 10 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x10 (16)
    
    // Field:   1
    // Width:   32
    // Count:   1
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF15 LMIN:0 LMAX:0 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:32 RID:10 RCNT:1
    // Locals:  USAG:FF150003 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF150003
    // Coll:
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF15: Vendor-defined
    uint32_t VEN_VendorDefined0003;                    // Usage 0xFF150003: , Value = 0 to 0
} HIDLuminanceNotificationInputReport10;

#endif /* IOHIDDeviceLuminanceNotificationDescriptor_h */
