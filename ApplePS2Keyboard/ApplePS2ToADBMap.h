/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLEPS2TOADBMAP_H
#define _APPLEPS2TOADBMAP_H

#define DEADKEY 0x80

static const UInt8 PS2ToADBMap[0x80] = 
{
/*  ADB       AT  Key-Legend
    ======================== */
    DEADKEY,  // 00
    0x35,  // 01  Escape
    0x12,  // 02  1
    0x13,  // 03  2
    0x14,  // 04  3
    0x15,  // 05  4
    0x17,  // 06  5
    0x16,  // 07  6
    0x1a,  // 08  7
    0x1c,  // 09  8
    0x19,  // 0a  9
    0x1d,  // 0b  0
    0x1b,  // 0c  -_
    0x18,  // 0d  =+
    0x33,  // 0e  Backspace
    0x30,  // 0f  Tab
    0x0c,  // 10  Q
    0x0d,  // 11  W
    0x0e,  // 12  E
    0x0f,  // 13  R
    0x11,  // 14  T
    0x10,  // 15  Y
    0x20,  // 16  U
    0x22,  // 17  I
    0x1f,  // 18  O
    0x23,  // 19  P
    0x21,  // 1a  [{
    0x1e,  // 1b  ]}
    0x24,  // 1c  Enter
    0x3b,  // 1d  Left Ctrl
    0x00,  // 1e  A
    0x01,  // 1f  S
    0x02,  // 20  D
    0x03,  // 21  F
    0x05,  // 22  G
    0x04,  // 23  H
    0x26,  // 24  J
    0x28,  // 25  K
    0x25,  // 26  L
    0x29,  // 27  ;:
    0x27,  // 28  '"
    0x32,  // 29  `~
    0x38,  // 2a  Left Shift
    0x2a,  // 2b  \|
    0x06,  // 2c  Z
    0x07,  // 2d  X
    0x08,  // 2e  C
    0x09,  // 2f  V
    0x0b,  // 30  B
    0x2d,  // 31  N
    0x2e,  // 32  M
    0x2b,  // 33  ,<
    0x2f,  // 34  .>
    0x2c,  // 35  /?
    0x3c,  // 36  Right Shift
    0x43,  // 37  Keypad *
    0x3a,  // 38  Left Alt
    0x31,  // 39  Space
    0x39,  // 3a  Caps Lock
    0x7a,  // 3b  F1
    0x78,  // 3c  F2
    0x63,  // 3d  F3
    0x76,  // 3e  F4
    0x60,  // 3f  F5
    0x61,  // 40  F6
    0x62,  // 41  F7
    0x64,  // 42  F8
    0x65,  // 43  F9
    0x6d,  // 44  F10
    0x47,  // 45  Num Lock
    0x6b,  // 46  Scroll Lock
    0x59,  // 47  Keypad Home
    0x5b,  // 48  Keypad Up
    0x5c,  // 49  Keypad PgUp
    0x4e,  // 4a  Keypad -
    0x56,  // 4b  Keypad Left
    0x57,  // 4c  Keypad 5
    0x58,  // 4d  Keypad Right
    0x45,  // 4e  Keypad +
    0x53,  // 4f  Keypad End
    0x54,  // 50  Keypad Down
    0x55,  // 51  Keypad PgDn
    0x52,  // 52  Keypad Insert
    0x41,  // 53  Keypad Del
    DEADKEY,  // 54  SysReq
    DEADKEY,  // 55
    DEADKEY,  // 56
    0x67,  // 57  F11
    0x6f,  // 58  F12
    DEADKEY,  // 59
    DEADKEY,  // 5a
    DEADKEY,  // 5b
    DEADKEY,  // 5c
    DEADKEY,  // 5d
    DEADKEY,  // 5e
    DEADKEY,  // 5f
    0x3e,  // 60  Right Ctrl
    0x3d,  // 61  Right Alt
    0x4c,  // 62  Keypad Enter
    0x4b,  // 53  Keypad /
    0x7e,  // 64  Up Arrow
    0x7d,  // 65  Down Arrow
    0x7b,  // 66  Left Arrow
    0x7c,  // 67  Right Arrow
    0x72,  // 68  Insert
    0x75,  // 69  Delete
    0x74,  // 6a  Page Up
    0x79,  // 6b  Page Down
    0x73,  // 6c  Home
    0x77,  // 6d  End
    0x69,  // 6e  Print Scrn
    0x71,  // 6f  Pause
    0x37,  // 70  Left Window
    0x36,  // 71  Right Window
    0x6e,  // 72  Applications
    DEADKEY,  // 73
    DEADKEY,  // 74
    DEADKEY,  // 75
    DEADKEY,  // 76
    DEADKEY,  // 77
    DEADKEY,  // 78
    DEADKEY,  // 79
    DEADKEY,  // 7a
    DEADKEY,  // 7b
    DEADKEY,  // 7c
    DEADKEY,  // 7d
    DEADKEY,  // 7e
    DEADKEY   // 7f
};

#endif /* !_APPLEPS2TOADBMAP_H */
