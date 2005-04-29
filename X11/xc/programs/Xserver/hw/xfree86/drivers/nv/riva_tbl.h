 /***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_tbl.h,v 1.10 2003/07/31 20:24:31 mvojkovi Exp $ */


/*
 * RIVA Fixed Functionality Init Tables.
 */
static unsigned RivaTablePMC[][2] =
{
    {0x00000050, 0x00000000},
    {0x00000080, 0xFFFF00FF},
    {0x00000080, 0xFFFFFFFF}
};
static unsigned RivaTablePTIMER[][2] =
{
    {0x00000080, 0x00000008},
    {0x00000084, 0x00000003},
    {0x00000050, 0x00000000},
    {0x00000040, 0xFFFFFFFF}
};
static unsigned RivaTableFIFO[][2] =
{
    {0x00000000, 0x80000000},
    {0x00000800, 0x80000001},
    {0x00001000, 0x80000002},
    {0x00001800, 0x80000010},
    {0x00002000, 0x80000011},
    {0x00002800, 0x80000012},
    {0x00003000, 0x80000016},
    {0x00003800, 0x80000013}
};
static unsigned nv3TablePFIFO[][2] =
{
    {0x00000140, 0x00000000},
    {0x00000480, 0x00000000},
    {0x00000490, 0x00000000},
    {0x00000494, 0x00000000},
    {0x00000481, 0x00000000},
    {0x00000084, 0x00000000},
    {0x00000086, 0x00002000},
    {0x00000085, 0x00002200},
    {0x00000484, 0x00000000},
    {0x0000049C, 0x00000000},
    {0x00000104, 0x00000000},
    {0x00000108, 0x00000000},
    {0x00000100, 0x00000000},
    {0x000004A0, 0x00000000},
    {0x000004A4, 0x00000000},
    {0x000004A8, 0x00000000},
    {0x000004AC, 0x00000000},
    {0x000004B0, 0x00000000},
    {0x000004B4, 0x00000000},
    {0x000004B8, 0x00000000},
    {0x000004BC, 0x00000000},
    {0x00000050, 0x00000000},
    {0x00000040, 0xFFFFFFFF},
    {0x00000480, 0x00000001},
    {0x00000490, 0x00000001},
    {0x00000140, 0x00000001}
};
static unsigned nv3TablePGRAPH[][2] =
{
    {0x00000020, 0x1230001F},
    {0x00000021, 0x10113000},
    {0x00000022, 0x1131F101},
    {0x00000023, 0x0100F531},
    {0x00000060, 0x00000000},
    {0x00000065, 0x00000000},
    {0x00000068, 0x00000000},
    {0x00000069, 0x00000000},
    {0x0000006A, 0x00000000},
    {0x0000006B, 0x00000000},
    {0x0000006C, 0x00000000},
    {0x0000006D, 0x00000000},
    {0x0000006E, 0x00000000},
    {0x0000006F, 0x00000000},
    {0x000001A8, 0x00000000},
    {0x00000440, 0xFFFFFFFF},
    {0x00000480, 0x00000001},
    {0x000001A0, 0x00000000},
    {0x000001A2, 0x00000000},
    {0x0000018A, 0xFFFFFFFF},
    {0x00000190, 0x00000000},
    {0x00000142, 0x00000000},
    {0x00000154, 0x00000000},
    {0x00000155, 0xFFFFFFFF},
    {0x00000156, 0x00000000},
    {0x00000157, 0xFFFFFFFF},
    {0x00000064, 0x10010002},
    {0x00000050, 0x00000000},
    {0x00000051, 0x00000000},
    {0x00000040, 0xFFFFFFFF},
    {0x00000041, 0xFFFFFFFF},
    {0x00000440, 0xFFFFFFFF},
    {0x000001A9, 0x00000001}
};
static unsigned nv3TablePGRAPH_8BPP[][2] =
{
    {0x000001AA, 0x00001111}
};
static unsigned nv3TablePGRAPH_15BPP[][2] =
{
    {0x000001AA, 0x00002222}
};
static unsigned nv3TablePGRAPH_32BPP[][2] =
{
    {0x000001AA, 0x00003333}
};
static unsigned nv3TablePRAMIN[][2] =
{
    {0x00000500, 0x00010000},
    {0x00000501, 0x007FFFFF},
    {0x00000200, 0x80000000},
    {0x00000201, 0x00C20341},
    {0x00000204, 0x80000001},
    {0x00000205, 0x00C50342},
    {0x00000208, 0x80000002},
    {0x00000209, 0x00C60343},
    {0x0000020C, 0x80000003},
    {0x0000020D, 0x00DC0348},
    {0x00000210, 0x80000004},
    {0x00000211, 0x00DC0349},
    {0x00000214, 0x80000005},
    {0x00000215, 0x00DC034A},
    {0x00000218, 0x80000006},
    {0x00000219, 0x00DC034B},
    {0x00000240, 0x80000010},
    {0x00000241, 0x00D10344},
    {0x00000244, 0x80000011},
    {0x00000245, 0x00D00345},
    {0x00000248, 0x80000012},
    {0x00000249, 0x00CC0346},
    {0x0000024C, 0x80000013},
    {0x0000024D, 0x00D70347},
    {0x00000258, 0x80000016},
    {0x00000259, 0x00CA034C},
    {0x00000D05, 0x00000000},
    {0x00000D06, 0x00000000},
    {0x00000D07, 0x00000000},
    {0x00000D09, 0x00000000},
    {0x00000D0A, 0x00000000},
    {0x00000D0B, 0x00000000},
    {0x00000D0D, 0x00000000},
    {0x00000D0E, 0x00000000},
    {0x00000D0F, 0x00000000},
    {0x00000D11, 0x00000000},
    {0x00000D12, 0x00000000},
    {0x00000D13, 0x00000000},
    {0x00000D15, 0x00000000},
    {0x00000D16, 0x00000000},
    {0x00000D17, 0x00000000},
    {0x00000D19, 0x00000000},
    {0x00000D1A, 0x00000000},
    {0x00000D1B, 0x00000000},
    {0x00000D1D, 0x00000140},
    {0x00000D1E, 0x00000000},
    {0x00000D1F, 0x00000000},
    {0x00000D20, 0x10100200},
    {0x00000D21, 0x00000000},
    {0x00000D22, 0x00000000},
    {0x00000D23, 0x00000000},
    {0x00000D24, 0x10210200},
    {0x00000D25, 0x00000000},
    {0x00000D26, 0x00000000},
    {0x00000D27, 0x00000000},
    {0x00000D28, 0x10420200},
    {0x00000D29, 0x00000000},
    {0x00000D2A, 0x00000000},
    {0x00000D2B, 0x00000000},
    {0x00000D2C, 0x10830200},
    {0x00000D2D, 0x00000000},
    {0x00000D2E, 0x00000000},
    {0x00000D2F, 0x00000000},
    {0x00000D31, 0x00000000},
    {0x00000D32, 0x00000000},
    {0x00000D33, 0x00000000}
};
static unsigned nv3TablePRAMIN_8BPP[][2] =
{
    /*           0xXXXXX3XX For  MSB mono format */
    /*           0xXXXXX2XX For  LSB mono format */
    {0x00000D04, 0x10110203},
    {0x00000D08, 0x10110203},
    {0x00000D0C, 0x1011020B},
    {0x00000D10, 0x10118203},
    {0x00000D14, 0x10110203},
    {0x00000D18, 0x10110203},
    {0x00000D1C, 0x10419208},
    {0x00000D30, 0x10118203}
};
static unsigned nv3TablePRAMIN_15BPP[][2] =
{
    /*           0xXXXXX2XX For  MSB mono format */
    /*           0xXXXXX3XX For  LSB mono format */
    {0x00000D04, 0x10110200},
    {0x00000D08, 0x10110200},
    {0x00000D0C, 0x10110208},
    {0x00000D10, 0x10118200},
    {0x00000D14, 0x10110200},
    {0x00000D18, 0x10110200},
    {0x00000D1C, 0x10419208},
    {0x00000D30, 0x10118200}
};
static unsigned nv3TablePRAMIN_32BPP[][2] =
{
    /*           0xXXXXX3XX For  MSB mono format */
    /*           0xXXXXX2XX For  LSB mono format */
    {0x00000D04, 0x10110201},
    {0x00000D08, 0x10110201},
    {0x00000D0C, 0x10110209},
    {0x00000D10, 0x10118201},
    {0x00000D14, 0x10110201},
    {0x00000D18, 0x10110201},
    {0x00000D1C, 0x10419208},
    {0x00000D30, 0x10118201}
};

