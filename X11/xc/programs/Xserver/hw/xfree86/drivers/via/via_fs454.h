/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_fs454.h,v 1.2 2003/08/27 15:16:09 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIA_FS454_H_
#define _VIA_FS454_H_ 1

static const VIABIOSFS454TVMASKTableRec fs454MaskTable = {
    { 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0, 0XFF, 0, 0, 0, 0, 0, 0, 0XFF, 0, 0XFF, 0, 0, 0XFF, 0XFF, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0, 0, 0, 0, 0, 0XFF, 0XFF, 0XFF, 0, 0, 0XFF, 0XFF, 0XFF, 0, 0, 0 },
    0X3F, 0X18, 13, 22
};

static const VIABIOSFS454TableRec fs454Table[] = {
    {
        { 0X1C, 0X48C4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X100E, 0X40F, 0X92, 0XB93, 0X3200, 0X1, 0X2A02, 0X3, 0, 0, 0 },
        { 0X70, 0X4F, 0X4F, 0X94, 0X5E, 0X88, 0X6F, 0XBA, 0, 0X40, 0, 0, 0, 0, 0, 0, 0X16, 0, 0XDF, 0, 0, 0XDF, 0X70, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X20, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0XF, 0X7F, 0X7F, 0XF, 0X9A, 0X23, 0X8F, 0XE5, 0X57, 0XDF, 0XDF, 0X57, 0X11, 0XA, 0XE7, 0X21, 0, 0, 0, 0, 0, 0X28, 0X50, 0, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XF, 0X7F, 0X7F, 0XF, 0X9A, 0X23, 0X8F, 0XE5, 0X57, 0XDF, 0XDF, 0X57, 0X11, 0XA, 0XE7, 0X21, 0, 0, 0, 0, 0, 0X50, 0XA0, 0X40, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XF, 0X7F, 0X7F, 0XF, 0X9A, 0X23, 0X8F, 0XE5, 0X57, 0XDF, 0XDF, 0X57, 0X11, 0XA, 0XE7, 0X21, 0, 0, 0, 0, 0, 0XA0, 0X40, 0X81, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X7, 0X2A02, 0XAF65, 0XAA66, 0XAA67, 0X9127, 0X9C2B, 0X272C, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X5065, 0X4B66, 0X4D67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0XBE16, 0X8717, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0X1C, 0X4CC4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X100E, 0X40F, 0X92, 0XB93, 0, 0X1, 0X2302, 0X3, 0, 0, 0 },
        { 0X79, 0X63, 0X63, 0X9D, 0X68, 0X95, 0X5, 0XF1, 0, 0X60, 0, 0, 0, 0, 0, 0, 0X9C, 0, 0X57, 0, 0, 0X57, 0X6, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X20, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0XEF, 0X1F, 0X1F, 0XEF, 0XDB, 0X33, 0X3F, 0XE7, 0X6, 0X57, 0X57, 0X6, 0X5A, 0X13, 0XAD, 0X5D, 0, 0, 0, 0, 0, 0X32, 0X64, 0, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XEF, 0X1F, 0X1F, 0XEF, 0XDB, 0X33, 0X3F, 0XE7, 0X6, 0X57, 0X57, 0X6, 0X5A, 0X13, 0XAD, 0X5D, 0, 0, 0, 0, 0, 0X64, 0XC8, 0X40, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XEF, 0X1F, 0X1F, 0XEF, 0XDB, 0X33, 0X3F, 0XE7, 0X6, 0X57, 0X57, 0X6, 0X5A, 0X13, 0XAD, 0X5D, 0, 0, 0, 0, 0, 0XC8, 0X90, 0X81, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X6, 0X2A02, 0X9365, 0X9066, 0X9067, 0X9127, 0X9C2B, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X4365, 0X4A66, 0X4967, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0X1216, 0X4917, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0X1C, 0X4EC4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X900E, 0X40F, 0X92, 0XB93, 0X1000, 0X1, 0X4602, 0X3, 0, 0, 0 },
        { 0X97, 0X7F, 0X7F, 0X9B, 0X85, 0XD, 0XE6, 0XF5, 0, 0X60, 0, 0, 0, 0, 0, 0, 0X52, 0, 0XFF, 0, 0, 0XFF, 0XE7, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0X87, 0XFF, 0XFF, 0X87, 0X23, 0X34, 0X9, 0X38, 0XB0, 0XFF, 0XFF, 0XB0, 0X9A, 0X13, 0X14, 0X7A, 0, 0, 0, 0, 0, 0X40, 0X80, 0, 0, 0, 0X80, 0X20, 0X9C, 0, 0, 0 },
        { 0X87, 0XFF, 0XFF, 0X87, 0X23, 0X34, 0X9, 0X38, 0XB0, 0XFF, 0XFF, 0XB0, 0X9A, 0X13, 0X14, 0X7A, 0, 0, 0, 0, 0, 0X80, 0, 0X41, 0, 0, 0X80, 0X20, 0X9C, 0, 0, 0 },
        { 0X87, 0XFF, 0XFF, 0X87, 0X23, 0X34, 0X9, 0X38, 0XB0, 0XFF, 0XFF, 0XB0, 0X9A, 0X13, 0X14, 0X7A, 0, 0, 0, 0, 0, 0, 0, 0X86, 0, 0, 0X80, 0X20, 0X9C, 0, 0, 0 },
        { 0X6, 0X2A02, 0XC265, 0XBE66, 0XBE67, 0X9127, 0X9C2B, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X5865, 0X4B66, 0X4A67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0X1816, 0X6117, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0X1C, 0X4EC4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X100E, 0X40F, 0X92, 0XB93, 0X1000, 0X1, 0X4602, 0X3, 0, 0, 0 },
        { 0X91, 0X69, 0X69, 0X95, 0X6D, 0X1, 0X43, 0X3E, 0, 0X40, 0, 0, 0, 0, 0, 0, 0XEF, 0, 0XDF, 0, 0, 0XDF, 0X44, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0XAF, 0X4F, 0X4F, 0XAF, 0XE3, 0X34, 0X67, 0XA4, 0X44, 0XDF, 0XDF, 0X44, 0X51, 0XA, 0XEF, 0X24, 0, 0, 0, 0, 0, 0X35, 0X6A, 0, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XAF, 0X4F, 0X4F, 0XAF, 0XE3, 0X34, 0X67, 0XA4, 0X44, 0XDF, 0XDF, 0X44, 0X51, 0XA, 0XEF, 0X24, 0, 0, 0, 0, 0, 0X6A, 0XD4, 0X40, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XAF, 0X4F, 0X4F, 0XAF, 0XE3, 0X34, 0X67, 0XA4, 0X44, 0XDF, 0XDF, 0X44, 0X51, 0XA, 0XEF, 0X24, 0, 0, 0, 0, 0, 0XD4, 0XA8, 0X81, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X6, 0X2A02, 0X9465, 0X9066, 0X9167, 0X9127, 0X9C2B, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X4465, 0X4566, 0X4567, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0X5B16, 0XA017, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0X1C, 0X4AC4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X100E, 0X40F, 0X92, 0XB93, 0X1D00, 0X1, 0X2A02, 0X3, 0, 0, 0 },
        { 0X70, 0X59, 0X59, 0X94, 0X66, 0X8A, 0X6F, 0XBA, 0, 0X40, 0, 0, 0, 0, 0, 0, 0X14, 0, 0XDF, 0, 0, 0XDF, 0X70, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X20, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0X47, 0XCF, 0XCF, 0X47, 0X9A, 0X23, 0XD9, 0XA, 0X57, 0XDF, 0XDF, 0X57, 0X51, 0XA, 0XFF, 0X3B, 0, 0, 0, 0, 0, 0X2D, 0X5A, 0, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X47, 0XCF, 0XCF, 0X47, 0X9A, 0X23, 0XD9, 0XA, 0X57, 0XDF, 0XDF, 0X57, 0X51, 0XA, 0XFF, 0X3B, 0, 0, 0, 0, 0, 0X5A, 0XB4, 0X40, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X47, 0XCF, 0XCF, 0X47, 0X9A, 0X23, 0XD9, 0XA, 0X57, 0XDF, 0XDF, 0X57, 0X51, 0XA, 0XFF, 0X3B, 0, 0, 0, 0, 0, 0XB4, 0X68, 0X81, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X6, 0X2A02, 0X9465, 0X9066, 0X9067, 0X9127, 0X9C2B, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X4265, 0X4966, 0X4867, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0XC316, 0X4C17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    }
};

static const VIABIOSFS454TableRec fs454OverTable[] = {
    {
        { 0X1C, 0X40C4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X100E, 0X40F, 0X92, 0XB93, 0X8000, 0X1, 0X1802, 0X3, 0, 0, 0 },
        { 0X70, 0X4F, 0X4F, 0X94, 0X5C, 0X84, 0XB, 0X3E, 0, 0X40, 0, 0, 0, 0, 0, 0, 0XEE, 0, 0XDF, 0, 0, 0XDF, 0XC, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X20, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0XCF, 0X7F, 0X7F, 0XCF, 0X92, 0X22, 0X87, 0XBC, 0X2F, 0XDF, 0XDF, 0X2F, 0X11, 0XA, 0XFF, 0X24, 0, 0, 0, 0, 0, 0X28, 0X50, 0, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XCF, 0X7F, 0X7F, 0XCF, 0X92, 0X22, 0X87, 0XBC, 0X2F, 0XDF, 0XDF, 0X2F, 0X11, 0XA, 0XFF, 0X24, 0, 0, 0, 0, 0, 0X50, 0XA0, 0X40, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0XCF, 0X7F, 0X7F, 0XCF, 0X92, 0X22, 0X87, 0XBC, 0X2F, 0XDF, 0XDF, 0X2F, 0X11, 0XA, 0XFF, 0X24, 0, 0, 0, 0, 0, 0XA0, 0X40, 0X81, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X4, 0X2A02, 0XAD65, 0XA966, 0XA967, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X4F65, 0X4966, 0X4967, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0XF416, 0X9F17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0X1C, 0X44C4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X100E, 0X40F, 0X92, 0XB93, 0X6000, 0X1, 0X1D02, 0X3, 0, 0, 0 },
        { 0X79, 0X63, 0X63, 0X9D, 0X6D, 0X95, 0X88, 0XF0, 0, 0X60, 0, 0, 0, 0, 0, 0, 0X58, 0, 0X57, 0, 0, 0X57, 0X89, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X20, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0X37, 0X1F, 0X1F, 0X37, 0XE3, 0X34, 0X4A, 0X8B, 0X98, 0X57, 0X57, 0X98, 0X52, 0X12, 0X59, 0X48, 0, 0, 0, 0, 0, 0X32, 0X64, 0, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X37, 0X1F, 0X1F, 0X37, 0XE3, 0X34, 0X4A, 0X8B, 0X98, 0X57, 0X57, 0X98, 0X52, 0X12, 0X59, 0X48, 0, 0, 0, 0, 0, 0X64, 0XC8, 0X40, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X37, 0X1F, 0X1F, 0X37, 0XE3, 0X34, 0X4A, 0X8B, 0X98, 0X57, 0X57, 0X98, 0X52, 0X12, 0X59, 0X48, 0, 0, 0, 0, 0, 0XC8, 0X90, 0X81, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X4, 0X2A02, 0X9365, 0X9066, 0X9067, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X4265, 0X4566, 0X4567, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0X1416, 0XC717, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0X1C, 0X46C4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X900E, 0X40F, 0X92, 0XB93, 0X2300, 0X1, 0X2502, 0X3, 0, 0, 0 },
        { 0X97, 0X7F, 0X7F, 0X9B, 0X89, 0X11, 0X37, 0XF5, 0, 0X60, 0, 0, 0, 0, 0, 0, 0X3, 0, 0XFF, 0, 0, 0XFF, 0X38, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0X40, 0X80, 0X16, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0XAF, 0XFF, 0XFF, 0XAF, 0X23, 0X34, 0XC, 0X49, 0X47, 0XFF, 0XFF, 0X47, 0X9A, 0X13, 0X4, 0X7A, 0, 0, 0, 0, 0, 0X40, 0X80, 0, 0, 0, 0X80, 0X20, 0X9C, 0, 0, 0 },
        { 0XAF, 0XFF, 0XFF, 0XAF, 0X23, 0X34, 0XC, 0X49, 0X47, 0XFF, 0XFF, 0X47, 0X9A, 0X13, 0X4, 0X7A, 0, 0, 0, 0, 0, 0X80, 0, 0X41, 0, 0, 0X80, 0X20, 0X9C, 0, 0, 0 },
        { 0XAF, 0XFF, 0XFF, 0XAF, 0X23, 0X34, 0XC, 0X49, 0X47, 0XFF, 0XFF, 0X47, 0X9A, 0X13, 0X4, 0X7A, 0, 0, 0, 0, 0, 0, 0, 0X86, 0, 0, 0X80, 0X20, 0X9C, 0, 0, 0 },
        { 0X4, 0X2A02, 0XC265, 0XBE66, 0XBE67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X5765, 0X4566, 0X4567, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0X6316, 0X3D17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0X1C, 0X42C4, 0X98C5, 0, 0X1, 0X100E, 0XF, 0X92, 0X393, 0XFA0, 0X2A1, 0X100E, 0X40F, 0X100E, 0XF, 0XFF9E, 0XFF9F, 0XA0, 0X3A1, 0X100C, 0X740D, 0X100E, 0X40F, 0X92, 0XB93, 0X5000, 0X1, 0X1802, 0X3, 0, 0, 0 },
        { 0X70, 0X59, 0X59, 0X94, 0X62, 0X8C, 0XB, 0X3E, 0, 0X40, 0, 0, 0, 0, 0, 0, 0XE8, 0, 0XDF, 0, 0, 0XDF, 0XC, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X20, 0X40, 0X80, 0, 0X47, 0X1C, 0, 0 },
        { 0, 0, 0, 0X47, 0X1C, 0, 0, 0 },
        { 0X1F, 0XCF, 0XCF, 0X1F, 0X9A, 0X23, 0XD6, 0XF6, 0XC, 0XDF, 0XDF, 0XC, 0X11, 0XA, 0XE1, 0X28, 0, 0, 0, 0, 0, 0X2D, 0X5A, 0, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X1F, 0XCF, 0XCF, 0X1F, 0X9A, 0X23, 0XD6, 0XF6, 0XC, 0XDF, 0XDF, 0XC, 0X11, 0XA, 0XE1, 0X28, 0, 0, 0, 0, 0, 0X5A, 0XB4, 0X40, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X1F, 0XCF, 0XCF, 0X1F, 0X9A, 0X23, 0XD6, 0XF6, 0XC, 0XDF, 0XDF, 0XC, 0X11, 0XA, 0XE1, 0X28, 0, 0, 0, 0, 0, 0XB4, 0X68, 0X81, 0, 0, 0X80, 0X20, 0X80, 0, 0, 0 },
        { 0X4, 0X2A02, 0XAB65, 0XA766, 0XA767, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X4, 0X302, 0X4D65, 0X4966, 0X4967, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0X3, 0X811, 0X1E16, 0X5C17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    }
};

#endif /* _VIA_FS454_H_ */
