#define HIDTouchPadKeyboard \
  0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */\
  0x09, 0x06,                  /* (LOCAL)  USAGE              0x00010006 Keyboard (CA=Application Collection)  */\
  0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010006: Page=Generic Desktop Page, Usage=Keyboard, Type=CA) */\
  0x85, 0x01,                  /*   (GLOBAL) REPORT_ID          0x01 (1)    */\
  0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */\
  0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
  0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */\
  0x19, 0xE0,                  /*   (LOCAL)  USAGE_MINIMUM      0x000700E0 Keyboard Left Control (DV=Dynamic Value)    */\
  0x29, 0xE3,                  /*   (LOCAL)  USAGE_MAXIMUM      0x000700E3 Keyboard Left GUI (DV=Dynamic Value)    */\
  0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
  0x95, 0x04,                  /*   (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields     */\
  0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (4 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
  0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 1 bit) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
  0x19, 0xE5,                  /*   (LOCAL)  USAGE_MINIMUM      0x000700E5 Keyboard Right Shift (DV=Dynamic Value)    */\
  0x29, 0xE7,                  /*   (LOCAL)  USAGE_MAXIMUM      0x000700E7 Keyboard Right GUI (DV=Dynamic Value)    */\
  0x95, 0x03,                  /*   (GLOBAL) REPORT_COUNT       0x03 (3) Number of fields     */\
  0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (3 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x26, 0xFF, 0x00,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)     */\
  0x19, 0x00,                  /*   (LOCAL)  USAGE_MINIMUM      0x00070000 Keyboard No event indicated (Sel=Selector)    */\
  0x2A, 0xFF, 0x00,            /*   (LOCAL)  USAGE_MAXIMUM      0x000700FF     */\
  0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */\
  0x95, 0x06,                  /*   (GLOBAL) REPORT_COUNT       0x06 (6) Number of fields     */\
  0x81, 0x00,                  /*   (MAIN)   INPUT              0x00000000 (6 fields x 8 bits) 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
  0x05, 0x0C,                  /*   (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page    */\
  0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
  0x09, 0x40,                  /*   (LOCAL)  USAGE              0x000C0040 Menu (OOC=On/Off Control)    */\
  0x09, 0x70,                  /*   (LOCAL)  USAGE              0x000C0070 Display Brightness Decrement (RTC=Re-trigger Control)    */\
  0x09, 0x6F,                  /*   (LOCAL)  USAGE              0x000C006F Display Brightness Increment (RTC=Re-trigger Control)    */\
  0x0A, 0xAE, 0x01,            /*   (LOCAL)  USAGE              0x000C01AE AL Keyboard Layout (Sel=Selector)    */\
  0x0A, 0x21, 0x02,            /*   (LOCAL)  USAGE              0x000C0221 AC Search (Sel=Selector)    */\
  0x09, 0xCF,                  /*   (LOCAL)  USAGE              0x000C00CF Voice Command (OSC=One Shot Control)    */\
  0x09, 0x65,                  /*   (LOCAL)  USAGE              0x000C0065 Snapshot (OSC=One Shot Control)    */\
  0x09, 0xB6,                  /*   (LOCAL)  USAGE              0x000C00B6 Scan Previous Track (OSC=One Shot Control)    */\
  0x09, 0xCD,                  /*   (LOCAL)  USAGE              0x000C00CD Play/Pause (OSC=One Shot Control)    */\
  0x09, 0xB5,                  /*   (LOCAL)  USAGE              0x000C00B5 Scan Next Track (OSC=One Shot Control)    */\
  0x09, 0xE2,                  /*   (LOCAL)  USAGE              0x000C00E2 Mute (OOC=On/Off Control)    */\
  0x09, 0xEA,                  /*   (LOCAL)  USAGE              0x000C00EA Volume Decrement (RTC=Re-trigger Control)    */\
  0x09, 0xE9,                  /*   (LOCAL)  USAGE              0x000C00E9 Volume Increment (RTC=Re-trigger Control)    */\
  0x09, 0x30,                  /*   (LOCAL)  USAGE              0x000C0030 Power (OOC=On/Off Control)    */\
  0x0A, 0x9D, 0x02,            /*   (LOCAL)  USAGE              0x000C029D AC Next Keyboard Layout Select (Sel=Selector)    */\
  0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
  0x95, 0x0F,                  /*   (GLOBAL) REPORT_COUNT       0x0F (15) Number of fields     */\
  0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (15 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
  0x81, 0x03,                  /*   (MAIN)   INPUT              0x00000003 (1 field x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x85, 0x02,                  /*   (GLOBAL) REPORT_ID          0x02 (2)    */\
  0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */\
  0x09, 0x66,                  /*   (LOCAL)  USAGE              0x00070066 Keyboard Power (Sel=Selector)    */\
  0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x95, 0x07,                  /*   (GLOBAL) REPORT_COUNT       0x07 (7) Number of fields     */\
  0x81, 0x03,                  /*   (MAIN)   INPUT              0x00000003 (7 fields x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x09, 0x66,                  /*   (LOCAL)  USAGE              0x00070066 Keyboard Power (Sel=Selector)    */\
  0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
  0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x95, 0x07,                  /*   (GLOBAL) REPORT_COUNT       0x07 (7) Number of fields     */\
  0xB1, 0x03,                  /*   (MAIN)   FEATURE            0x00000003 (7 fields x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0xC0,                        /* (MAIN)   END_COLLECTION     Application */\
  0x05, 0x0D,                  /* (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page  */\
  0x09, 0x05,                  /* (LOCAL)  USAGE              0x000D0005 Touch Pad (CA=Application Collection)  */\
  0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x000D0005: Page=Digitizer Device Page, Usage=Touch Pad, Type=CA) */\
  0x85, 0x03,                  /*   (GLOBAL) REPORT_ID          0x03 (3)    */\
  0x27, 0xFF, 0xFF, 0x00, 0x00, /*   (GLOBAL) LOGICAL_MAXIMUM    0x0000FFFF (65535)     */\
  0x09, 0x56,                  /*   (LOCAL)  USAGE              0x000D0056 Relative Scan Time (DV=Dynamic Value)    */\
  0x75, 0x10,                  /*   (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field     */\
  0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */\
  0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
  0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
  0x09, 0x57,                  /*   (LOCAL)  USAGE              0x000D0057     */\
  0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x25, 0x7F,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x7F (127)     */\
  0x75, 0x07,                  /*   (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field     */\
  0x09, 0xA1,                  /*   (LOCAL)  USAGE              0x000D00A1     */\
  0xB1, 0x02,                  /*   (MAIN)   FEATURE            0x00000002 (1 field x 7 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x05, 0x09,                  /*   (GLOBAL) USAGE_PAGE         0x0009 Button Page    */\
  0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */\
  0x19, 0x01,                  /*   (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
  0x29, 0x02,                  /*   (LOCAL)  USAGE_MAXIMUM      0x00090002 Button 2 Secondary (MULTI=Selector, On/Off, Momentary, or One Shot)    */\
  0x95, 0x02,                  /*   (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields     */\
  0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */\
  0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */\
  0x95, 0x06,                  /*   (GLOBAL) REPORT_COUNT       0x06 (6) Number of fields     */\
  0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (6 fields x 1 bit) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */\
  0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
  0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
  0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL) <-- Warning: USAGE type should be CP (Physical)   */\
  0x09, 0x42,                  /*     (LOCAL)  USAGE              0x000D0042 Tip Switch (MC=Momentary Control)      */\
  0x09, 0x47,                  /*     (LOCAL)  USAGE              0x000D0047 Confidence (DV=Dynamic Value)      */\
  0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x25, 0x05,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x05 (5)       */\
  0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
  0x75, 0x06,                  /*     (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field       */\
  0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 6 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
  0x46, 0x99, 0x03,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0399 (921)       */\
  0x35, 0x00,                  /*     (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34     */\
  0x26, 0x51, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0451 (1105)       */\
  0x75, 0x0C,                  /*     (GLOBAL) REPORT_SIZE        0x0C (12) Number of bits per field       */\
  0x55, 0x0E,                  /*     (GLOBAL) UNIT_EXPONENT      0x0E (Unit Value x 10⁻²)      */\
  0x65, 0x11,                  /*     (GLOBAL) UNIT               0x00000011 Distance in metres [1 cm units] (1=System=SI Linear, 1=Length=Centimetre)      */\
  0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x46, 0xFA, 0x01,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x01FA (506)       */\
  0x26, 0x5F, 0x02,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x025F (607)       */\
  0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
  0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
  0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
  0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL) <-- Warning: USAGE type should be CP (Physical)   */\
  0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
  0x09, 0x42,                  /*     (LOCAL)  USAGE              0x000D0042 Tip Switch (MC=Momentary Control)      */\
  0x09, 0x47,                  /*     (LOCAL)  USAGE              0x000D0047 Confidence (DV=Dynamic Value)      */\
  0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
  0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
  0x25, 0x05,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x05 (5)       */\
  0x75, 0x06,                  /*     (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field       */\
  0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 6 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
  0x46, 0x99, 0x03,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0399 (921)       */\
  0x26, 0x51, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0451 (1105)       */\
  0x75, 0x0C,                  /*     (GLOBAL) REPORT_SIZE        0x0C (12) Number of bits per field       */\
  0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x46, 0xFA, 0x01,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x01FA (506)       */\
  0x26, 0x5F, 0x02,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x025F (607)       */\
  0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
  0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
  0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
  0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL) <-- Warning: USAGE type should be CP (Physical)   */\
  0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
  0x09, 0x42,                  /*     (LOCAL)  USAGE              0x000D0042 Tip Switch (MC=Momentary Control)      */\
  0x09, 0x47,                  /*     (LOCAL)  USAGE              0x000D0047 Confidence (DV=Dynamic Value)      */\
  0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
  0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
  0x25, 0x05,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x05 (5)       */\
  0x75, 0x06,                  /*     (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field       */\
  0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 6 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
  0x46, 0x99, 0x03,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0399 (921)       */\
  0x26, 0x51, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0451 (1105)       */\
  0x75, 0x0C,                  /*     (GLOBAL) REPORT_SIZE        0x0C (12) Number of bits per field       */\
  0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x46, 0xFA, 0x01,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x01FA (506)       */\
  0x26, 0x5F, 0x02,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x025F (607)       */\
  0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
  0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
  0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
  0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL) <-- Warning: USAGE type should be CP (Physical)   */\
  0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
  0x09, 0x42,                  /*     (LOCAL)  USAGE              0x000D0042 Tip Switch (MC=Momentary Control)      */\
  0x09, 0x47,                  /*     (LOCAL)  USAGE              0x000D0047 Confidence (DV=Dynamic Value)      */\
  0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
  0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
  0x25, 0x05,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x05 (5)       */\
  0x75, 0x06,                  /*     (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field       */\
  0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 6 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
  0x46, 0x99, 0x03,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0399 (921)       */\
  0x26, 0x51, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0451 (1105)       */\
  0x75, 0x0C,                  /*     (GLOBAL) REPORT_SIZE        0x0C (12) Number of bits per field       */\
  0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x46, 0xFA, 0x01,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x01FA (506)       */\
  0x26, 0x5F, 0x02,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x025F (607)       */\
  0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
  0x05, 0x0D,                  /*   (GLOBAL) USAGE_PAGE         0x000D Digitizer Device Page    */\
  0x09, 0x22,                  /*   (LOCAL)  USAGE              0x000D0022 Finger (CL=Logical Collection)    */\
  0xA1, 0x00,                  /*   (MAIN)   COLLECTION         0x00 Physical (Usage=0x000D0022: Page=Digitizer Device Page, Usage=Finger, Type=CL) <-- Warning: USAGE type should be CP (Physical)   */\
  0x25, 0x01,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)       */\
  0x09, 0x42,                  /*     (LOCAL)  USAGE              0x000D0042 Tip Switch (MC=Momentary Control)      */\
  0x09, 0x47,                  /*     (LOCAL)  USAGE              0x000D0047 Confidence (DV=Dynamic Value)      */\
  0x75, 0x01,                  /*     (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field       */\
  0x95, 0x02,                  /*     (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (2 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x09, 0x38,                  /*     (LOCAL)  USAGE              0x000D0038 Transducer Index (DV=Dynamic Value)      */\
  0x25, 0x05,                  /*     (GLOBAL) LOGICAL_MAXIMUM    0x05 (5)       */\
  0x75, 0x06,                  /*     (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field       */\
  0x95, 0x01,                  /*     (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields       */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 6 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x05, 0x01,                  /*     (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page      */\
  0x46, 0x99, 0x03,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x0399 (921)       */\
  0x26, 0x51, 0x04,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x0451 (1105)       */\
  0x75, 0x0C,                  /*     (GLOBAL) REPORT_SIZE        0x0C (12) Number of bits per field       */\
  0x09, 0x30,                  /*     (LOCAL)  USAGE              0x00010030 X (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0x46, 0xFA, 0x01,            /*     (GLOBAL) PHYSICAL_MAXIMUM   0x01FA (506)       */\
  0x26, 0x5F, 0x02,            /*     (GLOBAL) LOGICAL_MAXIMUM    0x025F (607)       */\
  0x09, 0x31,                  /*     (LOCAL)  USAGE              0x00010031 Y (DV=Dynamic Value)      */\
  0x81, 0x02,                  /*     (MAIN)   INPUT              0x00000002 (1 field x 12 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap      */\
  0xC0,                        /*   (MAIN)   END_COLLECTION     Physical   */\
  0xC0,                        /* (MAIN)   END_COLLECTION     Application */\

//--------------------------------------------------------------------------------
// Keyboard/Keypad Page HIDTouchPadKeyboardFeatureReport 02 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
  uint8_t  reportId;                                 // Report ID = 0x02 (2)

  // Field:   1
  // Width:   1
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:02 RCNT:1
  // Locals:  USAG:00070066 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00070066
  // Coll:    Keyboard
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0007: Keyboard/Keypad Page
                                                     // Collection: Keyboard
  uint8_t  KB_KeyboardKeyboardPower : 1;             // Usage 0x00070066: Keyboard Power, Value = 0 to 1

  // Field:   2
  // Width:   1
  // Count:   7
  // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:02 RCNT:7
  // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:
  // Coll:    Keyboard
  // Access:  Read/Only
  // Type:    Variable
                                                     // Page 0x0007: Keyboard/Keypad Page
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
} HIDTouchPadKeyboardFeatureReport02;


//--------------------------------------------------------------------------------
// Keyboard/Keypad Page HIDTouchPadKeyboardInputReport 01 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
  uint8_t  reportId;                                 // Report ID = 0x01 (1)

  // Field:   1
  // Width:   1
  // Count:   4
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:4
  // Locals:  USAG:0 UMIN:000700E0 UMAX:000700E3 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000700E0 000700E1 000700E2 000700E3
  // Coll:    Keyboard
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0007: Keyboard/Keypad Page
                                                     // Collection: Keyboard
  uint8_t  KB_KeyboardKeyboardLeftControl : 1;       // Usage 0x000700E0: Keyboard Left Control, Value = 0 to 1
  uint8_t  KB_KeyboardKeyboardLeftShift : 1;         // Usage 0x000700E1: Keyboard Left Shift, Value = 0 to 1
  uint8_t  KB_KeyboardKeyboardLeftAlt : 1;           // Usage 0x000700E2: Keyboard Left Alt, Value = 0 to 1
  uint8_t  KB_KeyboardKeyboardLeftGui : 1;           // Usage 0x000700E3: Keyboard Left GUI, Value = 0 to 1

  // Field:   2
  // Width:   1
  // Count:   1
  // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
  // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:1
  // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:
  // Coll:    Keyboard
  // Access:  Read/Only
  // Type:    Array
                                                     // Page 0x0007: Keyboard/Keypad Page
  uint8_t  : 1;                                      // Pad

  // Field:   3
  // Width:   1
  // Count:   3
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:3
  // Locals:  USAG:0 UMIN:000700E5 UMAX:000700E7 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000700E5 000700E6 000700E7
  // Coll:    Keyboard
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0007: Keyboard/Keypad Page
  uint8_t  KB_KeyboardKeyboardRightShift : 1;        // Usage 0x000700E5: Keyboard Right Shift, Value = 0 to 1
  uint8_t  KB_KeyboardKeyboardRightAlt : 1;          // Usage 0x000700E6: Keyboard Right Alt, Value = 0 to 1
  uint8_t  KB_KeyboardKeyboardRightGui : 1;          // Usage 0x000700E7: Keyboard Right GUI, Value = 0 to 1

  // Field:   4
  // Width:   8
  // Count:   6
  // Flags:   00000000: 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
  // Globals: PAGE:0007 LMIN:0 LMAX:255 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:8 RID:01 RCNT:6
  // Locals:  USAG:0 UMIN:00070000 UMAX:000700FF DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00070000 00070001 00070002 00070003 00070004 00070005 00070006 00070007 00070008 00070009 0007000A 0007000B 0007000C 0007000D 0007000E 0007000F 00070010 00070011 00070012 00070013 00070014 00070015 00070016 00070017 00070018 00070019 0007001A 0007001B 0007001C 0007001D 0007001E 0007001F 00070020 00070021 00070022 00070023 00070024 00070025 00070026 00070027 00070028 00070029 0007002A 0007002B 0007002C 0007002D 0007002E 0007002F 00070030 00070031 00070032 00070033 00070034 00070035 00070036 00070037 00070038 00070039 0007003A 0007003B 0007003C 0007003D 0007003E 0007003F 00070040 00070041 00070042 00070043 00070044 00070045 00070046 00070047 00070048 00070049 0007004A 0007004B 0007004C 0007004D 0007004E 0007004F 00070050 00070051 00070052 00070053 00070054 00070055 00070056 00070057 00070058 00070059 0007005A 0007005B 0007005C 0007005D 0007005E 0007005F 00070060 00070061 00070062 00070063 00070064 00070065 00070066 00070067 00070068 00070069 0007006A 0007006B 0007006C 0007006D 0007006E 0007006F 00070070 00070071 00070072 00070073 00070074 00070075 00070076 00070077 00070078 00070079 0007007A 0007007B 0007007C 0007007D 0007007E 0007007F 00070080 00070081 00070082 00070083 00070084 00070085 00070086 00070087 00070088 00070089 0007008A 0007008B 0007008C 0007008D 0007008E 0007008F 00070090 00070091 00070092 00070093 00070094 00070095 00070096 00070097 00070098 00070099 0007009A 0007009B 0007009C 0007009D 0007009E 0007009F 000700A0 000700A1 000700A2 000700A3 000700A4 000700A5 000700A6 000700A7 000700A8 000700A9 000700AA 000700AB 000700AC 000700AD 000700AE 000700AF 000700B0 000700B1 000700B2 000700B3 000700B4 000700B5 000700B6 000700B7 000700B8 000700B9 000700BA 000700BB 000700BC 000700BD 000700BE 000700BF 000700C0 000700C1 000700C2 000700C3 000700C4 000700C5 000700C6 000700C7 000700C8 000700C9 000700CA 000700CB 000700CC 000700CD 000700CE 000700CF 000700D0 000700D1 000700D2 000700D3 000700D4 000700D5 000700D6 000700D7 000700D8 000700D9 000700DA 000700DB 000700DC 000700DD 000700DE 000700DF 000700E0 000700E1 000700E2 000700E3 000700E4 000700E5 000700E6 000700E7 000700E8 000700E9 000700EA 000700EB 000700EC 000700ED 000700EE 000700EF 000700F0 000700F1 000700F2 000700F3 000700F4 000700F5 000700F6 000700F7 000700F8 000700F9 000700FA 000700FB 000700FC 000700FD 000700FE 000700FF
  // Coll:    Keyboard
  // Access:  Read/Write
  // Type:    Array
                                                     // Page 0x0007: Keyboard/Keypad Page
  uint8_t  KB_Keyboard[6];                           // Value = 0 to 255

  // Field:   5
  // Width:   1
  // Count:   15
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:15
  // Locals:  USAG:000C029D UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000C0040 000C0070 000C006F 000C01AE 000C0221 000C00CF 000C0065 000C00B6 000C00CD 000C00B5 000C00E2 000C00EA 000C00E9 000C0030 000C029D
  // Coll:    Keyboard
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000C: Consumer Device Page
  uint8_t  CD_KeyboardMenu : 1;                      // Usage 0x000C0040: Menu, Value = 0 to 1
  uint8_t  CD_KeyboardDisplayBrightnessDecrement : 1; // Usage 0x000C0070: Display Brightness Decrement, Value = 0 to 1
  uint8_t  CD_KeyboardDisplayBrightnessIncrement : 1; // Usage 0x000C006F: Display Brightness Increment, Value = 0 to 1
  uint8_t  CD_KeyboardAlKeyboardLayout : 1;          // Usage 0x000C01AE: AL Keyboard Layout, Value = 0 to 1
  uint8_t  CD_KeyboardAcSearch : 1;                  // Usage 0x000C0221: AC Search, Value = 0 to 1
  uint8_t  CD_KeyboardVoiceCommand : 1;              // Usage 0x000C00CF: Voice Command, Value = 0 to 1
  uint8_t  CD_KeyboardSnapshot : 1;                  // Usage 0x000C0065: Snapshot, Value = 0 to 1
  uint8_t  CD_KeyboardScanPreviousTrack : 1;         // Usage 0x000C00B6: Scan Previous Track, Value = 0 to 1
  uint8_t  CD_KeyboardPlayPause : 1;                 // Usage 0x000C00CD: Play/Pause, Value = 0 to 1
  uint8_t  CD_KeyboardScanNextTrack : 1;             // Usage 0x000C00B5: Scan Next Track, Value = 0 to 1
  uint8_t  CD_KeyboardMute : 1;                      // Usage 0x000C00E2: Mute, Value = 0 to 1
  uint8_t  CD_KeyboardVolumeDecrement : 1;           // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
  uint8_t  CD_KeyboardVolumeIncrement : 1;           // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
  uint8_t  CD_KeyboardPower : 1;                     // Usage 0x000C0030: Power, Value = 0 to 1
  uint8_t  CD_KeyboardAcNextKeyboardLayoutSelect : 1; // Usage 0x000C029D: AC Next Keyboard Layout Select, Value = 0 to 1

  // Field:   6
  // Width:   1
  // Count:   1
  // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:01 RCNT:1
  // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:
  // Coll:    Keyboard
  // Access:  Read/Only
  // Type:    Variable
                                                     // Page 0x000C: Consumer Device Page
  uint8_t  : 1;                                      // Pad
} HIDTouchPadKeyboardInputReport01;


//--------------------------------------------------------------------------------
// Keyboard/Keypad Page HIDTouchPadKeyboardInputReport 02 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
  uint8_t  reportId;                                 // Report ID = 0x02 (2)

  // Field:   7
  // Width:   1
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:02 RCNT:1
  // Locals:  USAG:00070066 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00070066
  // Coll:    Keyboard
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0007: Keyboard/Keypad Page
  uint8_t  KB_KeyboardKeyboardPower : 1;             // Usage 0x00070066: Keyboard Power, Value = 0 to 1

  // Field:   8
  // Width:   1
  // Count:   7
  // Flags:   00000003: 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0007 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:02 RCNT:7
  // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:
  // Coll:    Keyboard
  // Access:  Read/Only
  // Type:    Variable
                                                     // Page 0x0007: Keyboard/Keypad Page
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
} HIDTouchPadKeyboardInputReport02;


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDTouchPadKeyboardFeatureReport 03 (Device <-> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
  uint8_t  reportId;                                 // Report ID = 0x03 (3)

  // Field:   1
  // Width:   1
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:03 RCNT:1
  // Locals:  USAG:000D0057 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0057
  // Coll:    TouchPad
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
                                                     // Collection: TouchPad
  uint8_t  DIG_TouchPadSurfaceSwitch : 1;                     // Usage 0x000D0057: , Value = 0 to 1

  // Field:   2
  // Width:   7
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:127 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:7 RID:03 RCNT:1
  // Locals:  USAG:000D00A1 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D00A1
  // Coll:    TouchPad
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadReportRate : 7;                     // Usage 0x000D00A1: , Value = 0 to 127
} HIDTouchPadKeyboardFeatureReport03;


//--------------------------------------------------------------------------------
// Digitizer Device Page HIDTouchPadKeyboardInputReport 03 (Device --> Host)
//--------------------------------------------------------------------------------

typedef struct __attribute__((packed))
{
  uint8_t  reportId;                                 // Report ID = 0x03 (3)

  // Field:   1
  // Width:   16
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:65535 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:16 RID:03 RCNT:1
  // Locals:  USAG:000D0056 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0056
  // Coll:    TouchPad
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
                                                     // Collection: TouchPad
  uint16_t DIG_TouchPadRelativeScanTime;             // Usage 0x000D0056: Relative Scan Time, Value = 0 to 65535

  // Field:   2
  // Width:   1
  // Count:   2
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:03 RCNT:2
  // Locals:  USAG:0 UMIN:00090001 UMAX:00090002 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00090001 00090002
  // Coll:    TouchPad
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0009: Button Page
  uint8_t  BTN_TouchPadButton1 : 1;                  // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1
  uint8_t  BTN_TouchPadButton2 : 1;                  // Usage 0x00090002: Button 2 Secondary, Value = 0 to 1

  // Field:   3
  // Width:   1
  // Count:   6
  // Flags:   00000001: 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull
  // Globals: PAGE:0009 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:03 RCNT:6
  // Locals:  USAG:0 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:
  // Coll:    TouchPad
  // Access:  Read/Only
  // Type:    Array
                                                     // Page 0x0009: Button Page
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad
  uint8_t  : 1;                                      // Pad

  // Field:   4
  // Width:   1
  // Count:   2
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:03 RCNT:2
  // Locals:  USAG:000D0047 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0042 000D0047
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
                                                     // Collection: TouchPad Finger
  uint8_t  DIG_TouchPadFingerTipSwitch : 1;          // Usage 0x000D0042: Tip Switch, Value = 0 to 1
  uint8_t  DIG_TouchPadFingerConfidence : 1;         // Usage 0x000D0047: Confidence, Value = 0 to 1

  // Field:   5
  // Width:   6
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:5 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:6 RID:03 RCNT:1
  // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0038
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTransducerIndex : 6;    // Usage 0x000D0038: Transducer Index, Value = 0 to 5

  // Field:   6
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:1105 PMIN:0 PMAX:921 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010030
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerX : 12;                  // Usage 0x00010030: X, Value = 0 to 1105, Physical = Value x 921 / 1105 in 10⁻⁴ m units

  // Field:   7
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:607 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010031
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerY : 12;                  // Usage 0x00010031: Y, Value = 0 to 607, Physical = Value x 506 / 607 in 10⁻⁴ m units

  // Field:   8
  // Width:   1
  // Count:   2
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:1 RID:03 RCNT:2
  // Locals:  USAG:000D0047 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0042 000D0047
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTipSwitch_1 : 1;        // Usage 0x000D0042: Tip Switch, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units
  uint8_t  DIG_TouchPadFingerConfidence_1 : 1;       // Usage 0x000D0047: Confidence, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units

  // Field:   9
  // Width:   6
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:5 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:6 RID:03 RCNT:1
  // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0038
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTransducerIndex_1 : 6;  // Usage 0x000D0038: Transducer Index, Value = 0 to 5, Physical = Value x 506 / 5 in 10⁻⁴ m units

  // Field:   10
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:1105 PMIN:0 PMAX:921 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010030
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerX_1 : 12;                // Usage 0x00010030: X, Value = 0 to 1105, Physical = Value x 921 / 1105 in 10⁻⁴ m units

  // Field:   11
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:607 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010031
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerY_1 : 12;                // Usage 0x00010031: Y, Value = 0 to 607, Physical = Value x 506 / 607 in 10⁻⁴ m units

  // Field:   12
  // Width:   1
  // Count:   2
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:1 RID:03 RCNT:2
  // Locals:  USAG:000D0047 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0042 000D0047
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTipSwitch_2 : 1;        // Usage 0x000D0042: Tip Switch, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units
  uint8_t  DIG_TouchPadFingerConfidence_2 : 1;       // Usage 0x000D0047: Confidence, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units

  // Field:   13
  // Width:   6
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:5 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:6 RID:03 RCNT:1
  // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0038
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTransducerIndex_2 : 6;  // Usage 0x000D0038: Transducer Index, Value = 0 to 5, Physical = Value x 506 / 5 in 10⁻⁴ m units

  // Field:   14
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:1105 PMIN:0 PMAX:921 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010030
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerX_2 : 12;                // Usage 0x00010030: X, Value = 0 to 1105, Physical = Value x 921 / 1105 in 10⁻⁴ m units

  // Field:   15
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:607 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010031
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerY_2 : 12;                // Usage 0x00010031: Y, Value = 0 to 607, Physical = Value x 506 / 607 in 10⁻⁴ m units

  // Field:   16
  // Width:   1
  // Count:   2
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:1 RID:03 RCNT:2
  // Locals:  USAG:000D0047 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0042 000D0047
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTipSwitch_3 : 1;        // Usage 0x000D0042: Tip Switch, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units
  uint8_t  DIG_TouchPadFingerConfidence_3 : 1;       // Usage 0x000D0047: Confidence, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units

  // Field:   17
  // Width:   6
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:5 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:6 RID:03 RCNT:1
  // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0038
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTransducerIndex_3 : 6;  // Usage 0x000D0038: Transducer Index, Value = 0 to 5, Physical = Value x 506 / 5 in 10⁻⁴ m units

  // Field:   18
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:1105 PMIN:0 PMAX:921 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010030
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerX_3 : 12;                // Usage 0x00010030: X, Value = 0 to 1105, Physical = Value x 921 / 1105 in 10⁻⁴ m units

  // Field:   19
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:607 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010031
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerY_3 : 12;                // Usage 0x00010031: Y, Value = 0 to 607, Physical = Value x 506 / 607 in 10⁻⁴ m units

  // Field:   20
  // Width:   1
  // Count:   2
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:1 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:1 RID:03 RCNT:2
  // Locals:  USAG:000D0047 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0042 000D0047
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTipSwitch_4 : 1;        // Usage 0x000D0042: Tip Switch, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units
  uint8_t  DIG_TouchPadFingerConfidence_4 : 1;       // Usage 0x000D0047: Confidence, Value = 0 to 1, Physical = Value x 506 in 10⁻⁴ m units

  // Field:   21
  // Width:   6
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:000D LMIN:0 LMAX:5 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:6 RID:03 RCNT:1
  // Locals:  USAG:000D0038 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  000D0038
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x000D: Digitizer Device Page
  uint8_t  DIG_TouchPadFingerTransducerIndex_4 : 6;  // Usage 0x000D0038: Transducer Index, Value = 0 to 5, Physical = Value x 506 / 5 in 10⁻⁴ m units

  // Field:   22
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:1105 PMIN:0 PMAX:921 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010030 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010030
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerX_4 : 12;                // Usage 0x00010030: X, Value = 0 to 1105, Physical = Value x 921 / 1105 in 10⁻⁴ m units

  // Field:   23
  // Width:   12
  // Count:   1
  // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
  // Globals: PAGE:0001 LMIN:0 LMAX:607 PMIN:0 PMAX:506 UEXP:-2 UNIT:00000011 RSIZ:12 RID:03 RCNT:1
  // Locals:  USAG:00010031 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
  // Usages:  00010031
  // Coll:    TouchPad Finger
  // Access:  Read/Write
  // Type:    Variable
                                                     // Page 0x0001: Generic Desktop Page
  uint16_t GD_TouchPadFingerY_4 : 12;                // Usage 0x00010031: Y, Value = 0 to 607, Physical = Value x 506 / 607 in 10⁻⁴ m units
} HIDTouchPadKeyboardInputReport03;
