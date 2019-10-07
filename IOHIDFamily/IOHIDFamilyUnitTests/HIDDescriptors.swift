//
//  HIDDescriptors.swift
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 8/15/18.
//

import Foundation

let HIDKeyboardDescriptor : [UInt8] = [
    0x05, 0x01,                  /* (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page  */
    0x09, 0x06,                  /* (LOCAL)  USAGE              0x00010006 Keyboard (CA=Application Collection)  */
    0xA1, 0x01,                  /* (MAIN)   COLLECTION         0x01 Application (Usage=0x00010006: Page=Generic Desktop Page, Usage=Keyboard, Type=CA) */
    0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */
    0x19, 0xE0,                  /*   (LOCAL)  USAGE_MINIMUM      0x000700E0 Keyboard Left Control (DV=Dynamic Value)    */
    0x29, 0xE7,                  /*   (LOCAL)  USAGE_MAXIMUM      0x000700E7 Keyboard Right GUI (DV=Dynamic Value)    */
    0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */
    0x25, 0x01,                  /*   (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)     */
    0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */
    0x95, 0x08,                  /*   (GLOBAL) REPORT_COUNT       0x08 (8) Number of fields     */
    0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (8 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */
    0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */
    0x81, 0x01,                  /*   (MAIN)   INPUT              0x00000001 (1 field x 8 bits) 1=Constant 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */
    0x05, 0x08,                  /*   (GLOBAL) USAGE_PAGE         0x0008 LED Indicator Page    */
    0x19, 0x01,                  /*   (LOCAL)  USAGE_MINIMUM      0x00080001 Num Lock (OOC=On/Off Control)    */
    0x29, 0x05,                  /*   (LOCAL)  USAGE_MAXIMUM      0x00080005 Kana (OOC=On/Off Control)    */
    0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */
    0x75, 0x01,                  /*   (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field     */
    0x91, 0x02,                  /*   (MAIN)   OUTPUT             0x00000002 (5 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */
    0x75, 0x03,                  /*   (GLOBAL) REPORT_SIZE        0x03 (3) Number of bits per field     */
    0x91, 0x01,                  /*   (MAIN)   OUTPUT             0x00000001 (1 field x 3 bits) 1=Constant 0=Array 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0x05, 0x07,                  /*   (GLOBAL) USAGE_PAGE         0x0007 Keyboard/Keypad Page    */
    0x19, 0x00,                  /*   (LOCAL)  USAGE_MINIMUM      0x00070000 Keyboard No event indicated (Sel=Selector)    */
    0x2A, 0xFF, 0x00,            /*   (LOCAL)  USAGE_MAXIMUM      0x000700FF     */
    0x95, 0x05,                  /*   (GLOBAL) REPORT_COUNT       0x05 (5) Number of fields     */
    0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field     */
    0x15, 0x00,                  /*   (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14   */
    0x26, 0xFF, 0x00,            /*   (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)     */
    0x81, 0x00,                  /*   (MAIN)   INPUT              0x00000000 (5 fields x 8 bits) 0=Data 0=Array 0=Absolute 0=Ignored 0=Ignored 0=PrefState 0=NoNull    */
    0x05, 0xFF,                  /*   (GLOBAL) USAGE_PAGE         0x00FF Reserved    */
    0x09, 0x03,                  /*   (LOCAL)  USAGE              0x00FF0003     */
    0x75, 0x08,                  /*   (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8    */
    0x95, 0x01,                  /*   (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields     */
    0x81, 0x02,                  /*   (MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap    */
    0xC0,                        /* (MAIN)   END_COLLECTION     Application */
]
