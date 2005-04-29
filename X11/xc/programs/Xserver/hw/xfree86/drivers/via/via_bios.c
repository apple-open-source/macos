/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_bios.c,v 1.11 2004/02/20 21:50:06 dawes Exp $ */
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

/*************************************************************************
 *
 *  File:       via_bios.c
 *  Content:    Get the mode table from VGA BIOS and set mode by this table
 *
 ************************************************************************/

/*
#define CREATE_MODETABLE_HEADERFILE
#define CREATE_TV2_HEADERFILE
#define CREATE_TV3_HEADERFILE
#define CREATE_SAA7108_HEADERFILE
#define CREATE_CH7019_HEADERFILE
#define CREATE_FS454_HEADERFILE
*/

#include "via_driver.h"
#ifndef CREATE_MODETABLE_HEADERFILE
#include "via_mode.h"
#endif
#ifndef CREATE_TV2_HEADERFILE
#include "via_tv2.h"
#endif
#ifndef CREATE_TV3_HEADERFILE
#include "via_tv3.h"
#endif
#ifndef CREATE_VT1622A_HEADERFILE
#include "via_vt1622a.h"
#endif
#ifndef CREATE_SAA7108_HEADERFILE
#include "via_saa7108.h"
#endif
#ifndef CREATE_CH7019_HEADERFILE
#include "via_ch7019.h"
#endif
#ifndef CREATE_FS454_HEADERFILE
#include "via_fs454.h"
#endif
#include "via_refresh.h"


/*=*
 *
 * int VIACheckTVExist(VIABIOSInfoPtr) - Check TV Endcoder
 *
 * Return Type:    int
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 0 - None
 *                 1 - VIA VT1621
 *                 2 - VIA VT1622
 *                 3 - Chrontel 7009
 *                 4 - Chrontel 7019
 *                 5 - Philips SAA7108
 *=*/

int VIACheckTVExist(VIABIOSInfoPtr pBIOSInfo)
{
    I2CDevPtr       dev;
    unsigned char   W_Buffer[1];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIACheckTVExist\n"));
    /* Check For TV2/TV3 */
    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0x40)) {
        dev = xf86CreateI2CDevRec();
        dev->DevName = "TV";
        dev->SlaveAddr = 0x40;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;

        if (xf86I2CDevInit(dev)) {
            W_Buffer[0] = 0x1B;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
            xf86DestroyI2CDevRec(dev,TRUE);
            switch (R_Buffer[0]) {
                case 2:
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                            "Found TVEncoder VT1621!\n"));
                    pBIOSInfo->TVEncoder = VIA_TV2PLUS;
                    pBIOSInfo->TVI2CAdd = 0x40;
                    break;
                case 3:
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                            "Found TVEncoder VT1622!\n"));
                    pBIOSInfo->TVEncoder = VIA_TV3;
                    pBIOSInfo->TVI2CAdd = 0x40;
                    break;
                case 16:
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                            "Found TVEncoder VT1622A!\n"));
                    pBIOSInfo->TVEncoder = VIA_VT1622A;
                    pBIOSInfo->TVI2CAdd = 0x40;
                    break;
                default:
                    pBIOSInfo->TVEncoder = VIA_NONETV;
                    break;
            }
        }
        else
            xf86DestroyI2CDevRec(dev,TRUE);
    }
    else
        pBIOSInfo->TVEncoder = VIA_NONETV;

    /* Check For SAA7108 */
    if (!pBIOSInfo->TVEncoder) {
        if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0x88)) {
            dev = xf86CreateI2CDevRec();
            dev->DevName = "SAA7108";
            dev->SlaveAddr = 0x88;
            dev->pI2CBus = pBIOSInfo->I2C_Port2;

            if (xf86I2CDevInit(dev)) {
                W_Buffer[0] = 0x1C;
                xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                xf86DestroyI2CDevRec(dev,TRUE);
                if (R_Buffer[0] == 0x04) {
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                            "Found TVEncoder Philips SAA7108!\n"));
                    pBIOSInfo->TVEncoder = VIA_SAA7108;
                    pBIOSInfo->TVI2CAdd = 0x88;
                }
                else
                    pBIOSInfo->TVEncoder = VIA_NONETV;
            }
            else
                xf86DestroyI2CDevRec(dev,TRUE);
            }
        else
            pBIOSInfo->TVEncoder = VIA_NONETV;
    }

	/* Check For FS454 */
	if (!pBIOSInfo->TVEncoder) {
	    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0xD4)) {
	        dev = xf86CreateI2CDevRec();
	        dev->DevName = "FS454";
	        dev->SlaveAddr = 0xD4;
	        dev->pI2CBus = pBIOSInfo->I2C_Port2;

	        if (xf86I2CDevInit(dev)) {
	            W_Buffer[0] = 0x7F;
	            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
	            xf86DestroyI2CDevRec(dev,TRUE);
	            if (R_Buffer[0] == 0x20) {
    				DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
    						"Found TVEncoder FOCUS 453/454!\n"));
                    pBIOSInfo->TVEncoder = VIA_FS454;
                    pBIOSInfo->TVI2CAdd	= 0xD4;
                }
                else
                    pBIOSInfo->TVEncoder = VIA_NONETV;
            }
            else
                xf86DestroyI2CDevRec(dev,TRUE);
            }
        else
            pBIOSInfo->TVEncoder = VIA_NONETV;
    }

	/* Check For CH7019 */
	if (!pBIOSInfo->TVEncoder) {
	    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0xEA)) {
	        dev = xf86CreateI2CDevRec();
	        dev->DevName = "CH7019";
	        dev->SlaveAddr = 0xEA;
	        dev->pI2CBus = pBIOSInfo->I2C_Port2;

	        if (xf86I2CDevInit(dev)) {
	            W_Buffer[0] = 0x4B;
	            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
	            xf86DestroyI2CDevRec(dev,TRUE);
	            if (R_Buffer[0] == 0x19) {
    				DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
    						"Found TVEncoder Chrontel 7019!\n"));
                    pBIOSInfo->TVEncoder = VIA_CH7019;
                    pBIOSInfo->TVI2CAdd	= 0xEA;
                }
				else
	                pBIOSInfo->TVEncoder = VIA_NONETV;
	        }
	        else
	            xf86DestroyI2CDevRec(dev,TRUE);
		    }
	    else
	        pBIOSInfo->TVEncoder = VIA_NONETV;
	}

    if (pBIOSInfo->TVEncoder == VIA_NONETV) {
        if (VIAGPIOI2C_Initial(pBIOSInfo, 0x40)) {
            VIAGPIOI2C_Read(pBIOSInfo, 0x1B, R_Buffer, 1);
            switch (R_Buffer[0]) {
                case 16:
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                            "Found TVEncoder VT1623!\n"));
                    pBIOSInfo->TVEncoder = VIA_VT1623;
                    break;
                default:
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                            "Unknow TVEncoder Type:%d!\n", R_Buffer[0]));
                    break;
            }
        }
    }
    return pBIOSInfo->TVEncoder;
}


/*=*
 *
 * log VIAQueryChipInfo(VIABIOSInfoPtr) - Query Chip Infomation
 *
 * Return Type:    log
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 [31:24] Major BIOS Version Number
 *                 [31:24] Minor BIOS Version Number
 *                 [15:8] Type of External TV Encoder
 *                     0 - None
 *                     1 - VIA VT1621
 *                     2 - VIA VT1622
 *                     3 - Chrontel 7009
 *                     4 - Chrontel 7019
 *                     5 - Philips SAA7108
 *                 [7:6] Reserved
 *                 [5] DVI Display Supported
 *                 [4] Reserved
 *                 [3] PAL TV Display Supported
 *                 [2] NTSC TV Display Supported
 *                 [1] LCD Display Supported
 *                 [0] CRT Display Supported
 *=*/

long VIAQueryChipInfo(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable;
    long            tmp;
    unsigned char   support = 0;

    pViaModeTable = pBIOSInfo->pModeTable;

    tmp = ((long)(pViaModeTable->BIOSVer)) << 16;
    tmp |= ((long)(VIACheckTVExist(pBIOSInfo))) << 8;

    if (VIA_CRT_SUPPORT)
        support |= VIA_CRT_SUPPORT_BIT;
    if (VIA_LCD_SUPPORT)
        support |= VIA_LCD_SUPPORT_BIT;
    if (VIA_NTSC_SUPPORT)
        support |= VIA_NTSC_SUPPORT_BIT;
    if (VIA_PAL_SUPPORT)
        support |= VIA_PAL_SUPPORT_BIT;
    if (VIA_DVI_SUPPORT)
        support |= VIA_DVI_SUPPORT_BIT;

    tmp |= (long)(support);

    return tmp;
}


/*=*
 *
 * char* VIAGetBIOSInfo(VIABIOSInfoPtr) - Get BIOS Release Date
 *
 * Return Type:    string pointer
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 BIOS release date string pointer
 *=*/

char* VIAGetBIOSInfo(VIABIOSInfoPtr pBIOSInfo)
{
    return pBIOSInfo->pModeTable->BIOSDate;
}


/*=*
 *
 * Bool VIASetActiveDisplay(VIABIOSInfoPtr, unsigned char)
 *
 *     - Set Active Display Device
 *
 * Return Type:    Bool
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 *                 Bit[7] 2nd Path
 *                 Bit[6] 1/0 MHS Enable/Disable
 *                 Bit[5] 0 = Bypass Callback, 1 = Enable Callback
 *                 Bit[4] 0 = Hot-Key Sequence Control (OEM Specific)
 *                 Bit[3] LCD
 *                 Bit[2] TV
 *                 Bit[1] CRT
 *                 Bit[0] DVI
 *
 * The Definition of Return Value:
 *
 *                 Success - TRUE
 *                 Not Success - FALSE
 *=*/

Bool VIASetActiveDisplay(VIABIOSInfoPtr pBIOSInfo, unsigned char display)
{
    VIABIOSInfoPtr  pVia;
    unsigned char tmp;

    pVia = pBIOSInfo;

    switch (display & 0x0F) {
        case 0x07:      /* WCRTON + WTVON + WDVION */
        case 0x0E:      /* WCRTON + WTVON + WLCDON */
            return FALSE;
        default:
            break;
    }

    VGAOUT8(0x3D4, 0x3E);
    tmp = VGAIN8(0x3D5) & 0xF0;
    tmp |= (display & 0x0F);
    VGAOUT8(0x3D5, tmp);

    if ((display & 0xC0) == 0x80)
        return FALSE;

    VGAOUT8(0x3D4, 0x3B);
    tmp = VGAIN8(0x3D5) & 0xE7;
    tmp |= ((display & 0xC0) >> 3);
    VGAOUT8(0x3D5, tmp);

    return TRUE;
}


/*=*
 *
 * unsigned char VIAGetActiveDisplay(VIABIOSInfoPtr, unsigned char)
 *
 *     - Get Active Display Device
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 Bit[7] 2nd Path
 *                 Bit[6] 1/0 MHS Enable/Disable
 *                 Bit[5] 0 = Bypass Callback, 1 = Enable Callback
 *                 Bit[4] 0 = Hot-Key Sequence Control (OEM Specific)
 *                 Bit[3] LCD
 *                 Bit[2] TV
 *                 Bit[1] CRT
 *                 Bit[0] DVI
 *=*/

unsigned char VIAGetActiveDisplay(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    unsigned char tmp;

    pVia = pBIOSInfo;

    VGAOUT8(0x3D4, 0x3E);
    tmp = (VGAIN8(0x3D5) & 0xF0) >> 4;

    VGAOUT8(0x3D4, 0x3B);
    tmp |= ((VGAIN8(0x3D5) & 0x18) << 3);

    return tmp;
}

/*=*
 *
 * unsigned char VIASensorCH7019(VIABIOSInfoPtr pBIOSInfo)
 *
 *     - Sense Chrontel 7019 Encoder
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                  00h  No Connected
 *                  02h  Composite
 *                  04h  S_VIDEO
 *                  06h  Composite + S_VIDEO
 *                  FFh  Undefine
 *=*/

unsigned char VIASensorCH7019(VIABIOSInfoPtr pBIOSInfo)
{
    unsigned char   tv20, ret = 0x06;
    I2CDevPtr       dev;
    unsigned char   W_Buffer[2];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASensorCH7019\n"));
    dev = xf86CreateI2CDevRec();
    dev->DevName = "CH7019";
    dev->SlaveAddr = 0xEA;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    if (xf86I2CDevInit(dev)) {
        /* turn all DACPD on  */
        W_Buffer[0] = 0x49;
        W_Buffer[1] = 0X40;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        /* set connection detect rgister (0x20) SENSE bit (BIT0) to 1 */
        W_Buffer[0] = 0x20;
        xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
        tv20 = R_Buffer[0];
        W_Buffer[1] = (tv20 | 0x01);
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        usleep(1);
        /* reset the SENSE bit to 0 */
        W_Buffer[1] = (tv20 & (~0x01));
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        /* read the status bits */
        xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
        xf86DestroyI2CDevRec(dev,TRUE);
    }
    else {
        xf86DestroyI2CDevRec(dev,TRUE);
    }
    /* Always return 0x06 */
    return ret;
}

/*=*
 *
 * unsigned char VIASensorSAA7108(VIABIOSInfoPtr pBIOSInfo)
 *
 *     - Sense Philips SAA7108 Encoder
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                  0               No Connected
 *                  BIT0=1          Composite
 *                  BIT1=1          S_VIDEO
 *                  BIT2=1          RGB
 *                  BIT3=1          YCbCr
 *                  FF              Undefine
 *=*/

unsigned char VIASensorSAA7108(VIABIOSInfoPtr pBIOSInfo)
{
    unsigned char   tv2d, tv61, tmp = 0, ret = 0;
    I2CDevPtr       dev;
    unsigned char   W_Buffer[2];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASensorSAA7108\n"));
    dev = xf86CreateI2CDevRec();
    dev->DevName = "SAA7108";
    dev->SlaveAddr = 0x88;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    if (xf86I2CDevInit(dev)) {

        /* Encoder On */
        W_Buffer[0] = 0x2D;
        xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
        tv2d = R_Buffer[0];
        W_Buffer[1] = 0XB4;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);

        /* Power Control */
        W_Buffer[0] = 0x61;
        xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
        tv61 = R_Buffer[0];
        W_Buffer[1] = (tv61 & (~0x60));
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);

        /* Monitor Sense Mode Threshold */
        W_Buffer[0] = 0x1A;
        W_Buffer[1] = 0X46;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);

        /* Monitor Sense On */
        W_Buffer[0] = 0x1B;
        W_Buffer[1] = 0X80;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        usleep(1);

        /* Monitor Sense Off */
        W_Buffer[1] = 0;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);

        /* Monitor Sense Result */
        xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
        tmp = R_Buffer[0] & 0x07;

        /* Restore Power Control & Encoder On/Off Status */
        W_Buffer[0] = 0x61;
        W_Buffer[1] = tv61;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        W_Buffer[0] = 0x2D;
        W_Buffer[1] = tv2d;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        xf86DestroyI2CDevRec(dev,TRUE);
    }
    else {
        xf86DestroyI2CDevRec(dev,TRUE);
    }

    switch (tmp) {
        case 0:     /* YCbCr */
            ret = 0x08;
            break;
        case 1:     /* S_VIDEO */
            ret = 0x02;
            break;
        case 6:     /* Composite */
            ret = 0x01;
            break;
        case 7:     /* No Connected */
            ret = 0;
            break;
        default:    /* Undefine */
            DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "Sense Result :%d\n", tmp));
            ret = 0xFF;
            break;
    }
    return ret;
}

/*=*
 *
 * unsigned char VIASensorTV2(VIABIOSInfoPtr pBIOSInfo)
 *
 *     - Sense TV2+ Encoder
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 Bit[2] Bit[0]
 *                 0      0      - Composite + S-Video
 *                 0      1      - S-Video
 *                 1      0      - Composite
 *                 1      1      - None
 *=*/

unsigned char VIASensorTV2(VIABIOSInfoPtr pBIOSInfo)
{
    I2CDevPtr       dev;
    unsigned char   save, tmp = 0x05;
    unsigned char   W_Buffer[2];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASensorTV2\n"));
    dev = xf86CreateI2CDevRec();
    dev->DevName = "VT1621";
    dev->SlaveAddr = 0x40;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    if (xf86I2CDevInit(dev)) {
        W_Buffer[0] = 0x0E;
        xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
        save = R_Buffer[0];
        W_Buffer[1] = 0x08;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        W_Buffer[1] = 0;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        W_Buffer[0] = 0x0F;
        xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
        tmp = R_Buffer[0] & 0x0F;
        W_Buffer[0] = 0x0E;
        W_Buffer[1] = save;
        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        xf86DestroyI2CDevRec(dev,TRUE);
    }
    else {
        xf86DestroyI2CDevRec(dev,TRUE);
    }

    return tmp;
}


/*=*
 *
 * unsigned char VIASensorTV3(VIABIOSInfoPtr pBIOSInfo)
 *
 *     - Sense TV3 Encoder
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 Bit[3] Bit[2] Bit[1] Bit[0]
 *                 1      1      1      1      - None
 *                 0      1      1      1      - Composite
 *                 1      1      1      0      - Composite
 *                                             - Others: S-Video
 *=*/

unsigned char VIASensorTV3(VIABIOSInfoPtr pBIOSInfo)
{
    I2CDevPtr       dev;
    unsigned char   save, tmp = 0;
    unsigned char   W_Buffer[2];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASensorTV3\n"));
    if (pBIOSInfo->TVEncoder == VIA_VT1623) {
        VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
        VIAGPIOI2C_Read(pBIOSInfo, 0x0E, R_Buffer, 1);
        save = R_Buffer[0];
        W_Buffer[0] = 0;
        VIAGPIOI2C_Write(pBIOSInfo, 0x0E, W_Buffer[0]);
        W_Buffer[0] = 0x80;
        VIAGPIOI2C_Write(pBIOSInfo, 0x0E, W_Buffer[0]);
        W_Buffer[0] = 0;
        VIAGPIOI2C_Write(pBIOSInfo, 0x0E, W_Buffer[0]);
        VIAGPIOI2C_Read(pBIOSInfo, 0x0F, R_Buffer, 1);
        tmp = R_Buffer[0] & 0x0F;
        W_Buffer[0] = save;
        VIAGPIOI2C_Write(pBIOSInfo, 0x0E, W_Buffer[0]);
    }
    else {
        dev = xf86CreateI2CDevRec();
        dev->DevName = "VT1622";
        dev->SlaveAddr = 0x40;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;

        if (xf86I2CDevInit(dev)) {
            W_Buffer[0] = 0x0E;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
            save = R_Buffer[0];
            W_Buffer[1] = 0;
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
            W_Buffer[1] = 0x80;
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
            W_Buffer[1] = 0;
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
            W_Buffer[0] = 0x0F;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
            tmp = R_Buffer[0] & 0x0F;
            W_Buffer[0] = 0x0E;
            W_Buffer[1] = save;
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
            xf86DestroyI2CDevRec(dev,TRUE);
        }
        else {
            xf86DestroyI2CDevRec(dev,TRUE);
        }
    }
    return tmp;
}


/*=*
 *
 * Bool VIASensorDVI(pBIOSInfo)
 *
 *     - Sense DVI Connector
 *
 * Return Type:    Bool
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 DVI Attached  - TRUE
 *                 DVI Not Attached - FALSE
 *=*/

Bool VIASensorDVI(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    unsigned char   SlaveAddr, cr6c, cr93;
    Bool            ret = FALSE;
    I2CDevPtr       dev;
    unsigned char   W_Buffer[1];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASensorDVI\n"));

    /* Enable DI0, DI1 */
    VGAOUT8(0x3d4, 0x6C);
    cr6c = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, cr6c | 0x21);
    VGAOUT8(0x3d4, 0x93);
    cr93 = VGAIN8(0x3d5);
    if (pBIOSInfo->ChipRev > 15) {
        VGAOUT8(0x3d5, 0xA3);
    }
    else {
        VGAOUT8(0x3d5, 0xBF);
    }

    /* Enable LCD */
    VIAEnableLCD(pBIOSInfo);

    switch (pBIOSInfo->TMDS) {
        case VIA_SIL164:
            SlaveAddr = 0x70;
            break;
        case VIA_VT3192:
            SlaveAddr = 0x10;
            break;
        default:
            return ret;
            break;
    }

    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, SlaveAddr)) {
        dev = xf86CreateI2CDevRec();
        dev->DevName = "TMDS";
        dev->SlaveAddr = SlaveAddr;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;

        if (xf86I2CDevInit(dev)) {
            W_Buffer[0] = 0x09;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
            xf86DestroyI2CDevRec(dev,TRUE);
            if (R_Buffer[0] & 0x04)
                ret = TRUE;
        }
        else
            xf86DestroyI2CDevRec(dev,TRUE);
    }

    if (pBIOSInfo->Chipset != VIA_CLE266) {
        VIAGPIOI2C_Initial(pBIOSInfo, SlaveAddr);
        VIAGPIOI2C_Read(pBIOSInfo, 0x09, R_Buffer, 1);
        if (R_Buffer[0] & 0x04)
            ret = TRUE;
    }

    /* Disable LCD */
    VIADisableLCD(pBIOSInfo);

    /* Restore DI0, DI1 status */
    VGAOUT8(0x3d4, 0x6C);
    VGAOUT8(0x3d5, cr6c);
    VGAOUT8(0x3d4, 0x93);
    VGAOUT8(0x3d5, cr93);

    return ret;
}

Bool VIAPostDVI(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    unsigned char   cr6c, cr93;
    Bool            ret = FALSE;
    I2CDevPtr       dev;
    unsigned char   W_Buffer[2];
    unsigned char   R_Buffer[4];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPostDVI\n"));

    /* Enable DI0, DI1 */
    VGAOUT8(0x3d4, 0x6C);
    cr6c = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, cr6c | 0x21);
    VGAOUT8(0x3d4, 0x93);
    cr93 = VGAIN8(0x3d5);
    if (pBIOSInfo->ChipRev > 15) {
        VGAOUT8(0x3d5, 0xA3);
    }
    else {
        VGAOUT8(0x3d5, 0xBF);
    }

    /* Enable LCD */
    VIAEnableLCD(pBIOSInfo);

    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0x70)) {
        dev = xf86CreateI2CDevRec();
        dev->DevName = "TMDS";
        dev->SlaveAddr = 0x70;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;
        if (xf86I2CDevInit(dev)) {
            W_Buffer[0] = 0x00;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,4);
            if (R_Buffer[0] == 0x06 && R_Buffer[1] == 0x11) {       /* This is VT3191 */
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                 "Found VIA LVDS Transmiter!\n"));
                if (R_Buffer[2] == 0x91 && R_Buffer[3] == 0x31) {
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                     "VIA: VT1631!\n"));
                    pBIOSInfo->LVDS = VIA_VT3191;
                    W_Buffer[0] = 0x08;
                    if (pBIOSInfo->BusWidth == VIA_DI_24BIT && pBIOSInfo->LCDDualEdge)
                        W_Buffer[1] = 0x0D;
                    else
                        W_Buffer[1] = 0x09;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    xf86DestroyI2CDevRec(dev,TRUE);
                    ret = TRUE;
                }
                else {
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                     "VIA: Unknow Chip!!\n"));
                    xf86DestroyI2CDevRec(dev,TRUE);
                }
            }
            else if (R_Buffer[0] == 0x01 && R_Buffer[1] == 0x00) {  /* This is Sil164 */
                W_Buffer[0] = 0x02;
                xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,2);
                if (R_Buffer[0] && R_Buffer[1]) {
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                     "Found TMDS Transmiter Silicon164.\n"));
                    pBIOSInfo->TMDS = VIA_SIL164;
                    W_Buffer[0] = 0x08;
                    if (pBIOSInfo->BusWidth == VIA_DI_24BIT) {
                        if (pBIOSInfo->LCDDualEdge)
                            W_Buffer[1] = 0x3F;
                        else
                            W_Buffer[1] = 0x37;
                    }
                    else    /* 12Bit Only has Single Mode */
                        W_Buffer[1] = 0x3B;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x0C;
                    W_Buffer[1] = 0x09;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    xf86DestroyI2CDevRec(dev,TRUE);
                    ret = TRUE;
                }
                else {
                    xf86DestroyI2CDevRec(dev,TRUE);
                }
            }
            else {
                xf86DestroyI2CDevRec(dev,TRUE);
            }
        }
        else {
            xf86DestroyI2CDevRec(dev,TRUE);
        }
    }

    /* Check VT3192 TMDS Exist or not?*/
    if (!pBIOSInfo->TMDS) {
        if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0x10)) {
            dev = xf86CreateI2CDevRec();
            dev->DevName = "TMDS";
            dev->SlaveAddr = 0x10;
            dev->pI2CBus = pBIOSInfo->I2C_Port2;
            if (xf86I2CDevInit(dev)) {
                W_Buffer[0] = 0x00;
                xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,4);

                if (R_Buffer[0] == 0x06 && R_Buffer[1] == 0x11) {   /* This is VT3192 */
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                     "Found VIA TMDS Transmiter!\n"));
                    pBIOSInfo->TMDS = VIA_VT3192;
                    if (R_Buffer[2] == 0x92 && R_Buffer[3] == 0x31) {
                        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                         "VIA: VT1632!\n"));
                        pBIOSInfo->TMDS = VIA_VT3192;
                        W_Buffer[0] = 0x08;
                        if (pBIOSInfo->BusWidth == VIA_DI_24BIT) {
                            if (pBIOSInfo->LCDDualEdge)
                                W_Buffer[1] = 0x3F;
                            else
                                W_Buffer[1] = 0x37;
                        }
                        else
                            W_Buffer[1] = 0x3B;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        xf86DestroyI2CDevRec(dev,TRUE);
                        ret = TRUE;
                    }
                    else {
                        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                         "VIA: Unknow Chip!!\n"));
                        xf86DestroyI2CDevRec(dev,TRUE);
                    }
                }
                else {
                    xf86DestroyI2CDevRec(dev,TRUE);
                }
            }
            else {
                xf86DestroyI2CDevRec(dev,TRUE);
            }
        }
    }

    /* Check CH7019 LVDS Exist or not?*/
    if (!pBIOSInfo->LVDS) {
        if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0xEA)) {
            dev = xf86CreateI2CDevRec();
            dev->DevName = "LVDS";
            dev->SlaveAddr = 0xEA;
            dev->pI2CBus = pBIOSInfo->I2C_Port2;
            if (xf86I2CDevInit(dev)) {
                W_Buffer[0] = 0x4B;
                xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                /* Load CH7019 LVDS init table */
                if (pBIOSInfo->LCDDualEdge || R_Buffer[0] == 0x3B) {   /* DUAL_INIT Table */
                    W_Buffer[0] = 0x64;
                    W_Buffer[1] = 0x14;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x71;
                    W_Buffer[1] = 0xE3;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x73;
                    W_Buffer[1] = 0xDB;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x74;
                    W_Buffer[1] = 0xF6;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x76;
                    W_Buffer[1] = 0xAF;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                }
                else if (R_Buffer[0] == 0x19 || R_Buffer[0] == 0x1B ||
                         R_Buffer[0] == 0x3A){      /* SINGLE_INIT Table */
                    W_Buffer[0] = 0x64;
                    W_Buffer[1] = 0x04;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x71;
                    W_Buffer[1] = 0xAD;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x73;
                    W_Buffer[1] = 0xC8;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x74;
                    W_Buffer[1] = 0xF3;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x76;
                    W_Buffer[1] = 0xAD;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                }
                /* COMMON_INIT Table */
                W_Buffer[0] = 0x63;
                W_Buffer[1] = 0x4B;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x1C;
                W_Buffer[1] = 0x40;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x6F;
                W_Buffer[1] = 0x00;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x21;
                W_Buffer[1] = 0x84;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x70;
                W_Buffer[1] = 0xC0;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x72;
                W_Buffer[1] = 0xAD;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x78;
                W_Buffer[1] = 0x20;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x15;
                W_Buffer[1] = 0x00;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                /* 2.set panel power sequence timing */
                W_Buffer[0] = 0x67;
                W_Buffer[1] = 0x01;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x68;
                W_Buffer[1] = 0x6E;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x69;
                W_Buffer[1] = 0x01;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x6A;
                W_Buffer[1] = 0x01;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x6B;
                W_Buffer[1] = 0x09;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                W_Buffer[0] = 0x66;
                W_Buffer[1] = 0x20;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);

                /* CH7017 support 24Bit panel */
                if (R_Buffer[0] == 0x1B || pBIOSInfo->BusWidth == VIA_DI_24BIT) {
                    /* Set LVDS output 24 bits mode*/
                    W_Buffer[0] = 0x64;
                    xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                    W_Buffer[1] = R_Buffer[0] | 0x20;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                }
#if 0
                for (i = 0; i < 10; i++) {
                    W_Buffer[0] = 0x63;
                    xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                    W_Buffer[1] = (R_Buffer[0] | 0x40);
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    usleep(1);
                    W_Buffer[1] &= ~0x40;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);

                    usleep(100);
                    W_Buffer[0] = 0x66;
                    xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                    if (((R_Buffer[0] & 0x44) == 0x44) || (i >= 9)) {  /* PLL lock OK, Turn on VDD */
                        if (i >= 9) {
                            usleep(500);
                        }
                        W_Buffer[1] = R_Buffer[0] | 0x01;
                        xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "CH7019 PLL lock ok!\n"));
                        break;
                    }
                }
#endif
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "Found Chrontel LVDS Transmiter!\n"));
                pBIOSInfo->LVDS = VIA_CH7019LVDS;
                /*pBIOSInfo->BusWidth = VIA_DI_12BIT;*/
                ret = TRUE;

                xf86DestroyI2CDevRec(dev,TRUE);
            }
            else {
                xf86DestroyI2CDevRec(dev,TRUE);
            }
        }
    }

    /* GPIO Sense */
    if (pBIOSInfo->Chipset != VIA_CLE266) {
        VIAGPIOI2C_Initial(pBIOSInfo, 0x70);
        VIAGPIOI2C_Read(pBIOSInfo, 0, R_Buffer, 2);
        if (R_Buffer[0] == 0x06 && R_Buffer[1] == 0x11) {   /* VIA LVDS */
            VIAGPIOI2C_Read(pBIOSInfo, 2, R_Buffer, 2);
            if (R_Buffer[0] == 0x91 && R_Buffer[1] == 0x31) {
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                 "Found LVDS Transmiter VT1631.\n"));
                pBIOSInfo->LVDS = VIA_VT3191;
                if (pBIOSInfo->BusWidth == VIA_DI_24BIT && pBIOSInfo->LCDDualEdge)
                    VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x0D);
                else
                    VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x09);
                ret = TRUE;
            }
        }
        else if (R_Buffer[0] == 0x01 && R_Buffer[1] == 0x0) {/* Silicon TMDS */
            VIAGPIOI2C_Read(pBIOSInfo, 2, R_Buffer, 2);
            if (R_Buffer[0] && R_Buffer[1]) {
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                 "Found TMDS Transmiter Silicon164.\n"));
                pBIOSInfo->TMDS = VIA_SIL164;
                if (pBIOSInfo->BusWidth == VIA_DI_24BIT) {
                    if (pBIOSInfo->LCDDualEdge)
                        VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x3F);
                    else
                        VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x37);
                }
                else {
                    VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x3B);
                }
                VIAGPIOI2C_Write(pBIOSInfo, 0x0C, 0x09);
                ret = TRUE;
            }
        }

        VIAGPIOI2C_Initial(pBIOSInfo, 0x10);
        VIAGPIOI2C_Read(pBIOSInfo, 0, R_Buffer, 2);
        if (R_Buffer[0] == 0x06 && R_Buffer[1] == 0x11) {   /* VIA TMDS */
            VIAGPIOI2C_Read(pBIOSInfo, 2, R_Buffer, 2);
            if (R_Buffer[0] == 0x92 && R_Buffer[1] == 0x31) {
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED,
                                 "Found TMDS Transmiter VT1632.\n"));
                pBIOSInfo->TMDS = VIA_VT3192;
                if (pBIOSInfo->BusWidth == VIA_DI_24BIT) {
                    if (pBIOSInfo->LCDDualEdge)
                        VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x3F);
                    else
                        VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x37);
                }
                else {
                    VIAGPIOI2C_Write(pBIOSInfo, 0x08, 0x3B);
                }
                ret = TRUE;
            }
        }
    }
    /* Disable LCD */
    VIADisableLCD(pBIOSInfo);

    /* Restore DI0, DI1 status */
    VGAOUT8(0x3d4, 0x6C);
    VGAOUT8(0x3d5, cr6c);
    VGAOUT8(0x3d4, 0x93);
    VGAOUT8(0x3d5, cr93);

    if (pBIOSInfo->LVDS && pBIOSInfo->PanelSize == VIA_PANEL_INVALID) {
        VGAOUT8(0x3d4, 0x3F);
        pBIOSInfo->PanelSize = (int)(VGAIN8(0x3d5) >> 4);
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO,
             "Get PanelID From Scratch Pad is %d\n", pBIOSInfo->PanelSize));
    }

    return ret;
}

/*=*
 *
 * unsigned char VIAGetDeviceDetect(VIABIOSInfoPtr)
 *
 *     - Get Display Device Attched
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 Bit[7] Reserved ------------ 2nd TV Connector
 *                 Bit[6] Reserved ------------ 1st TV Connector
 *                 Bit[5] Reserved
 *                 Bit[4] CRT2
 *                 Bit[3] DFP
 *                 Bit[2] TV
 *                 Bit[1] LCD
 *                 Bit[0] CRT
 *=*/

unsigned char VIAGetDeviceDetect(VIABIOSInfoPtr pBIOSInfo)
{
    unsigned char tmp, sense;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAGetDeviceDetect\n"));

    tmp = VIA_DEVICE_CRT1; /* Default assume color CRT attached */
/*
    if (VIA_LCD_SUPPORT) {
        if (VIA_UNCOVERD_LCD_PANEL)
            tmp |= 0x08;
    }
*/
    if (pBIOSInfo->LVDS) {
        pBIOSInfo->LCDAttach = TRUE;
        tmp |= VIA_DEVICE_LCD;
    }

    switch (pBIOSInfo->TVEncoder) {
        case VIA_NONETV:
            pBIOSInfo->TVAttach = FALSE;
            break;
        case VIA_TV2PLUS:
            sense = VIASensorTV2(pBIOSInfo);
            if (sense == 0x05) {
                pBIOSInfo->TVAttach = FALSE; /* No TV Output Attached */
            }
            else {
                tmp |= VIA_DEVICE_TV;
                pBIOSInfo->TVAttach = TRUE;
                if (!pBIOSInfo->TVOutput) {
                    if (sense == 0) {
                        /*tmp |= 0xC0;  Connect S_Video + Composite */
                        pBIOSInfo->TVOutput = TVOUTPUT_SC;
                    }
                    if (sense == 0x01) {
                        /*tmp |= 0x80;  Connect S_Video */
                        pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
                    }
                    else {
                        /*tmp |= 0x40;  Connect Composite */
                        pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
                    }
                }
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "TV2 sense = %d ,TVOutput = %d\n", sense, pBIOSInfo->TVOutput));
            }
            break;
        case VIA_TV3:
        case VIA_VT1622A:
        case VIA_VT1623:
            sense = VIASensorTV3(pBIOSInfo);
            if (sense == 0x0F) {
                pBIOSInfo->TVAttach = FALSE; /* No TV Output Attached */
            }
            else {
                tmp |= VIA_DEVICE_TV;
                pBIOSInfo->TVAttach = TRUE;
                if (!pBIOSInfo->TVOutput) {
                    if (sense == 0x07 || sense == 0x0E) {
                        /*tmp |= 0x40;  Connect Composite */
                        pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
                    }
                    else {
                        /*tmp |= 0x80;  Connect S_Video */
                        pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
                    }
                }
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "TV3 sense = %d ,TVOutput = %d\n", sense, pBIOSInfo->TVOutput));
            }
            break;
        case VIA_CH7009:
        case VIA_CH7019:
            sense = VIASensorCH7019(pBIOSInfo);
            if (!sense) {
                pBIOSInfo->TVAttach = FALSE; /* No TV Output Attached */
            }
            else {
                tmp |= VIA_DEVICE_TV;
                pBIOSInfo->TVAttach = TRUE;
                if (!pBIOSInfo->TVOutput) {
                    if (sense == 0x02) {
                        pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
                    }
                    else {
                        pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
                    }
                }
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "CH7019 sense = %d ,TVOutput = %d\n", sense, pBIOSInfo->TVOutput));
            }
            break;
        case VIA_SAA7108:
            sense = VIASensorSAA7108(pBIOSInfo);
            if (!sense) {
                pBIOSInfo->TVAttach = FALSE; /* No TV Output Attached */
            }
            else {
                tmp |= VIA_DEVICE_TV;
                pBIOSInfo->TVAttach = TRUE;
                if (!pBIOSInfo->TVOutput) {
                    switch (sense) {
                        case 0x01:
                            pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
                            break;
                        case 0x02:
                            pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
                            break;
                        case 0x04:
                            pBIOSInfo->TVOutput = TVOUTPUT_RGB;
                            break;
                        case 0x08:
                            pBIOSInfo->TVOutput = TVOUTPUT_YCBCR;
                            break;
                        default:
                            pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
                            break;
                    }
                }
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "SAA7108 sense = %d ,TVOutput = %d\n", sense, pBIOSInfo->TVOutput));
            }
            break;
        case VIA_FS454:
        	tmp |= VIA_DEVICE_TV;
        	pBIOSInfo->TVAttach = TRUE;
        	break;
        default:
            break;
    }

    if (pBIOSInfo->TMDS) {
        if (VIASensorDVI(pBIOSInfo)) {
            tmp |= VIA_DEVICE_DFP;
            pBIOSInfo->DVIAttach = TRUE;
            DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "DVI has Attachment.\n"));
            if (pBIOSInfo->PanelSize == VIA_PANEL_INVALID)
                VIAGetPanelInfo(pBIOSInfo);
        }
        else {
            pBIOSInfo->DVIAttach = FALSE;
            DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "DVI hasn't Attachment.\n"));
        }
    }

    if ((pBIOSInfo->Chipset == VIA_KM400) && !(tmp & VIA_DEVICE_LCD)) {
	/* there currently is no infrastructure to check if another device
	   is already using this i2cbus, this will be solved later when
	   the whole i2c output detection is reworked.
	   VT1622 TV encoder does not reply to DDC/EDID. */

	/* if there is a hardwired panel attached then the second i2cbus 
	   will respond to DDC/EDID address probe (at least on I2CScans 
	   i have seen from acer aspires) */

	if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0xA0) || 
	    xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0xA2)) {
	    tmp |= VIA_DEVICE_LCD;
	    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "DDC/EDID response on I2C_Port2 on km400/kn400: assume hardwired panel.\n"));
	}
    }

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "Returning %d.\n", tmp));
    return tmp;
}


/*=*
 *
 * Bool VIASetPanelState(VIABIOSInfoPtr, unsigned char)
 *
 *     - Set Flat Panel Expaension/Centering State
 *
 * Return Type:    Bool
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 *                 Bit[7:1] Reserved
 *                 Bit[0] 0/1 = Centering/Expansion
 *
 * The Definition of Return Value:
 *
 *                 Success - TRUE
 *                 Not Success - FALSE
 *=*/

Bool VIASetPanelState(VIABIOSInfoPtr pBIOSInfo, unsigned char state)
{
    VIABIOSInfoPtr  pVia;
    unsigned char tmp;

    pVia = pBIOSInfo;

    tmp = state & 0x01;
    VGAOUT8(0x3D4, 0x3B);
    tmp |= (VGAIN8(0x3D5) & 0xFE);
    VGAOUT8(0x3D5, tmp);

    return TRUE;
}


/*=*
 *
 * unsigned char VIAGetPanelState(VIABIOSInfoPtr)
 *
 *     - Get Flat Panel Expaension/Centering State
 *
 * Return Type:    unsigend char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 *                 Bit[7:1] Reserved
 *                 Bit[0] 0/1 = Centering/Expansion
 *
 * The Definition of Return Value:
 *
 *                 Bit[7:1] Reserved
 *                 Bit[0] 0/1 = Centering/Expansion
 *=*/

unsigned char VIAGetPanelState(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    unsigned char tmp;

    pVia = pBIOSInfo;

    VGAOUT8(0x3D4, 0x3B);
    tmp = VGAIN8(0x3D5) & 0x01;

    return tmp;
}


/*=*
 *
 * int VIAQueryDVIEDID(void)
 *
 *     - Query Flat Panel's EDID Table Version Through DVI Connector
 *
 * Return Type:    int
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 0 - Found No EDID Table
 *                 1 - Found EDID1 Table
 *                 2 - Found EDID2 Table
 *=*/

int VIAQueryDVIEDID(VIABIOSInfoPtr pBIOSInfo)
{
    I2CDevPtr       dev;
    unsigned char   W_Buffer[1];
    unsigned char   R_Buffer[2];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAQueryDVIEDID\n"));
    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0xA0)) {
        pBIOSInfo->dev = xf86CreateI2CDevRec();
        dev = pBIOSInfo->dev;
        dev->DevName = "EDID1";
        dev->SlaveAddr = 0xA0;
        dev->ByteTimeout = 2200; /* VESA DDC spec 3 p. 43 (+10 %) */
        dev->StartTimeout = 550;
        dev->BitTimeout = 40;
        dev->ByteTimeout = 40;
        dev->AcknTimeout = 40;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;

        if (xf86I2CDevInit(dev)) {
            W_Buffer[0] = 0;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,2);
            if ((R_Buffer[0] == 0) && (R_Buffer[1] == 0xFF))
                return 1; /* Found EDID1 Table */
            else
                xf86DestroyI2CDevRec(dev,TRUE);
        }
        else {
            xf86DestroyI2CDevRec(dev,TRUE);
        }
    }

    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, 0xA2)) {
        pBIOSInfo->dev = xf86CreateI2CDevRec();
        dev = pBIOSInfo->dev;
        dev->DevName = "EDID2";
        dev->SlaveAddr = 0xA2;
        dev->ByteTimeout = 2200; /* VESA DDC spec 3 p. 43 (+10 %) */
        dev->StartTimeout = 550;
        dev->BitTimeout = 40;
        dev->ByteTimeout = 40;
        dev->AcknTimeout = 40;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;

        if (xf86I2CDevInit(dev)) {
            W_Buffer[0] = 0;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
            if (R_Buffer[0] == 0x20)
                return 2; /* Found EDID2 Table */
            else {
                xf86DestroyI2CDevRec(dev,TRUE);
                return 0; /* Found No EDID Table */
            }
        }
        else {
            xf86DestroyI2CDevRec(dev,TRUE);
            return 0; /* Found No EDID Table */
        }
    }

    if (pBIOSInfo->Chipset != VIA_CLE266) {
        VIAGPIOI2C_Initial(pBIOSInfo, 0xA0);
        VIAGPIOI2C_Read(pBIOSInfo, 0, R_Buffer, 2);
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "R_Buffer[0]=%d\n", R_Buffer[0]));
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "R_Buffer[1]=%d\n", R_Buffer[1]));
        if ((R_Buffer[0] == 0) && (R_Buffer[1] == 0xFF))
            return 1; /* Found EDID1 Table */
        else {
            VIAGPIOI2C_Initial(pBIOSInfo, 0xA2);
            VIAGPIOI2C_Read(pBIOSInfo, 0, R_Buffer, 1);
            DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "R_Buffer[0]=%d\n", R_Buffer[0]));
            if (R_Buffer[0] == 0x20)
                return 2; /* Found EDID2 Table */
            else
                return 0; /* Found No EDID Table */
        }
    }

        return 0;
}


/*=*
 *
 * unsigned char VIAGetPanelSizeFromDDCv1(VIABIOSInfoPtr pBIOSInfo)
 *
 *     - Get Panel Size Using EDID1 Table
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 0 - 640x480
 *                 1 - 800x600
 *                 2 - 1024x768
 *                 3 - 1280x768
 *                 4 - 1280x1024
 *                 5 - 1400x1050
 *                 6 - 1600x1200
 *                 0xFF - Not Supported Panel Size
 *=*/

unsigned char VIAGetPanelSizeFromDDCv1(VIABIOSInfoPtr pBIOSInfo)
{
    ScrnInfoPtr     pScrn = xf86Screens[pBIOSInfo->scrnIndex];
    xf86MonPtr      pMon;
    int             i, max = 0;
    unsigned char   W_Buffer[1];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAGetPanelSizeFromDDCv1\n"));
    for (i = 0x23; i < 0x35; i++) {
        switch (i) {
            case 0x23:
                if (pBIOSInfo->Chipset == VIA_CLE266) {
                    W_Buffer[0] = i;
                    xf86I2CWriteRead(pBIOSInfo->dev, W_Buffer,1, R_Buffer,1);
                }
                else {
                    VIAGPIOI2C_Read(pBIOSInfo, i, R_Buffer, 1);
                }
                if (R_Buffer[0] & 0x3C)
                    max = 640;
                if (R_Buffer[0] & 0xC0)
                    max = 720;
                if (R_Buffer[0] & 0x03)
                    max = 800;
                break;
            case 0x24:
                if (pBIOSInfo->Chipset == VIA_CLE266) {
                    W_Buffer[0] = i;
                    xf86I2CWriteRead(pBIOSInfo->dev, W_Buffer,1, R_Buffer,1);
                }
                else {
                    VIAGPIOI2C_Read(pBIOSInfo, i, R_Buffer, 1);
                }
                if (R_Buffer[0] & 0xC0)
                    max = 800;
                if (R_Buffer[0] & 0x1E)
                    max = 1024;
                if (R_Buffer[0] & 0x01)
                    max = 1280;
                break;
            case 0x26:
            case 0x28:
            case 0x2A:
            case 0x2C:
            case 0x2E:
            case 0x30:
            case 0x32:
            case 0x34:
                if (pBIOSInfo->Chipset == VIA_CLE266) {
                    W_Buffer[0] = i;
                    xf86I2CWriteRead(pBIOSInfo->dev, W_Buffer,1, R_Buffer,1);
                }
                else {
                    VIAGPIOI2C_Read(pBIOSInfo, i, R_Buffer, 1);
                }
                if (R_Buffer[0] == 1)
                    break;
                R_Buffer[0] += 31;
                R_Buffer[0] = R_Buffer[0] << 3; /* data = (data + 31) * 8 */
                if (R_Buffer[0] > max)
                    max = R_Buffer[0];
                break;
            default:
                break;
        }
    }

    if (pBIOSInfo->Chipset == VIA_CLE266) {
    xf86DestroyI2CDevRec(pBIOSInfo->dev,TRUE);

    pMon = xf86DoEDID_DDC2(pScrn->scrnIndex, pBIOSInfo->I2C_Port2);
    if (pMon) {
        pBIOSInfo->DDC2 =  pMon;
        xf86PrintEDID(pMon);
        xf86SetDDCproperties(pScrn, pMon);
        for (i = 0; i < 8; i++) {
            if (pMon->timings2[i].hsize > max) {
                max = pMon->timings2[i].hsize;
            }
        }
        if (pBIOSInfo->DDC1) {
            xf86SetDDCproperties(pScrn, pBIOSInfo->DDC1);
        }
    }
    }

    switch (max) {
        case 640:
            pBIOSInfo->PanelSize = VIA_PANEL6X4;
            break;
        case 800:
            pBIOSInfo->PanelSize = VIA_PANEL8X6;
            break;
        case 1024:
            pBIOSInfo->PanelSize = VIA_PANEL10X7;
            break;
        case 1280:
            pBIOSInfo->PanelSize = VIA_PANEL12X10;
            break;
        case 1400:
            pBIOSInfo->PanelSize = VIA_PANEL14X10;
            break;
        case 1600:
            pBIOSInfo->PanelSize = VIA_PANEL16X12;
            break;
        default:
            pBIOSInfo->PanelSize = VIA_PANEL_INVALID;
            break;
    }
    return pBIOSInfo->PanelSize;
}


/*=*
 *
 * unsigned char VIAGetPanelSizeFromDDCv2(VIABIOSInfoPtr pBIOSInfo)
 *
 *     - Get Panel Size Using EDID2 Table
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 0 - 640x480
 *                 1 - 800x600
 *                 2 - 1024x768
 *                 3 - 1280x768
 *                 4 - 1280x1024
 *                 5 - 1400x1050
 *                 6 - 1600x1200
 *                 0xFF - Not Supported Panel Size
 *=*/

unsigned char VIAGetPanelSizeFromDDCv2(VIABIOSInfoPtr pBIOSInfo)
{
    int             data = 0;
    unsigned char   W_Buffer[1];
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAGetPanelSizeFromDDCv2\n"));
    if (pBIOSInfo->Chipset == VIA_CLE266) {
    W_Buffer[0] = 0x77;
    xf86I2CWriteRead(pBIOSInfo->dev, W_Buffer,1, R_Buffer,1);
    data = R_Buffer[0];
    data = data << 8;
    W_Buffer[0] = 0x76;
    xf86I2CWriteRead(pBIOSInfo->dev, W_Buffer,1, R_Buffer,1);
    data |= R_Buffer[0];

    xf86DestroyI2CDevRec(pBIOSInfo->dev,TRUE);
    }
    else {
        VIAGPIOI2C_Read(pBIOSInfo, 0x76, R_Buffer, 2);
        data = R_Buffer[0];
        data += R_Buffer[1] << 8;
    }

    switch (data) {
        case 640:
            pBIOSInfo->PanelSize = VIA_PANEL6X4;
            break;
        case 800:
            pBIOSInfo->PanelSize = VIA_PANEL8X6;
            break;
        case 1024:
            pBIOSInfo->PanelSize = VIA_PANEL10X7;
            break;
        case 1280:
            pBIOSInfo->PanelSize = VIA_PANEL12X10;
            break;
        case 1400:
            pBIOSInfo->PanelSize = VIA_PANEL14X10;
            break;
        case 1600:
            pBIOSInfo->PanelSize = VIA_PANEL16X12;
            break;
        default:
            pBIOSInfo->PanelSize = VIA_PANEL_INVALID;
            break;
    }
    return pBIOSInfo->PanelSize;
}

/*=*
 *
 * unsigned char VIAGetPanelInfo(VIABIOSInfoPtr pBIOSInfo)
 *
 *     - Get Panel Size
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 VIABIOSInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 0 - 640x480
 *                 1 - 800x600
 *                 2 - 1024x768
 *                 3 - 1280x768
 *                 4 - 1280x1024
 *                 5 - 1400x1050
 *                 6 - 1600x1200
 *                 0xFF - Not Supported Panel Size
 *=*/

unsigned char VIAGetPanelInfo(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    unsigned char   cr6c, cr93;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAGetPanelInfo\n"));

    if (pBIOSInfo->PanelSize == VIA_PANEL_INVALID) {

        /* Enable DI0, DI1 */
        VGAOUT8(0x3d4, 0x6C);
        cr6c = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, cr6c | 0x21);
        VGAOUT8(0x3d4, 0x93);
        cr93 = VGAIN8(0x3d5);
        if (pBIOSInfo->ChipRev > 15) {
            VGAOUT8(0x3d5, 0xA3);
        }
        else {
            VGAOUT8(0x3d5, 0xBF);
        }

        /* Enable LCD */
        VIAEnableLCD(pBIOSInfo);

        switch (VIAQueryDVIEDID(pBIOSInfo)) {
            case 1:
                VIAGetPanelSizeFromDDCv1(pBIOSInfo);
                break;
            case 2:
                VIAGetPanelSizeFromDDCv2(pBIOSInfo);
                break;
            default:
                break;
        }

        /* Disable LCD */
        VIADisableLCD(pBIOSInfo);

        /* Restore DI0, DI1 status */
        VGAOUT8(0x3d4, 0x6C);
        VGAOUT8(0x3d5, cr6c);
        VGAOUT8(0x3d4, 0x93);
        VGAOUT8(0x3d5, cr93);
    }

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "PanelSize = %d\n", pBIOSInfo->PanelSize));
    return (unsigned char)(pBIOSInfo->PanelSize);
}

#ifdef CREATE_MODETABLE_HEADERFILE
Bool VIACreateHeaderFile(VIAModeTablePtr pViaModeTable)
{
    int                     i, j, k, m;
    int                     numMPatch;
    VIALCDMPatchEntryPtr    MPatch;
    FILE                    *pFile;


    if ((pFile = fopen("via_mode.h", "w+")) == NULL) {
        ErrorF("Can't open \"via_mode.h\" file!!\n");
        return FALSE;
    }

    fprintf(pFile, "#ifndef _VIA_MODETABLE_H\n");
    fprintf(pFile, "#define _VIA_MODETABLE_H\n");
    fprintf(pFile, "\n");

    fprintf(pFile, "static const unsigned short BIOSVer = %#X;\n\n", pViaModeTable->BIOSVer);
    fprintf(pFile, "static char BIOSDate[] = { ");
    for (i = 0; i < 9; i++) {
        if (i == 8)
            fprintf(pFile, "%#X };\n\n", pViaModeTable->BIOSDate[i]);
        else
            fprintf(pFile, "%#X, ", pViaModeTable->BIOSDate[i]);
    }
    fprintf(pFile, "static const unsigned short NumModes = %d;\n\n", pViaModeTable->NumModes);
    fprintf(pFile, "static const int NumPowerOn = %d;\n\n", pViaModeTable->NumPowerOn);
    fprintf(pFile, "static const int NumPowerOff = %d;\n\n", pViaModeTable->NumPowerOff);
    fprintf(pFile, "static VIAModeEntry Modes[] = {\n");
    for (i = 0; i < pViaModeTable->NumModes; i++) {
        fprintf(pFile, "    { %d, %d, %d, %#X, %d, %#X, %#X, ",
                pViaModeTable->Modes[i].Width,
                pViaModeTable->Modes[i].Height,
                pViaModeTable->Modes[i].Bpp,
                pViaModeTable->Modes[i].Mode,
                pViaModeTable->Modes[i].MemNeed,
                pViaModeTable->Modes[i].MClk,
                pViaModeTable->Modes[i].VClk);
        fprintf(pFile, "{ %d, %d, %d, %#X, ", /* stdVgaTable */
                pViaModeTable->Modes[i].stdVgaTable.columns,
                pViaModeTable->Modes[i].stdVgaTable.rows,
                pViaModeTable->Modes[i].stdVgaTable.fontHeight,
                pViaModeTable->Modes[i].stdVgaTable.pageSize);
        fprintf(pFile, "{ "); /* SR[] */
        for (j = 0; j < 5; j++) {
            if (j == 4)
                fprintf(pFile, "%#X }, ", /* SR[] */
                        pViaModeTable->Modes[i].stdVgaTable.SR[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].stdVgaTable.SR[j]);
        }
        fprintf(pFile, "%#X, ",
                pViaModeTable->Modes[i].stdVgaTable.misc);
        fprintf(pFile, "{ "); /* CR[] */
        for (j = 0; j < 25; j++) {
            if (j == 24)
                fprintf(pFile, "%#X }, ", /* CR[] */
                        pViaModeTable->Modes[i].stdVgaTable.CR[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].stdVgaTable.CR[j]);
        }
        fprintf(pFile, "{ "); /* AR[] */
        for (j = 0; j < 20; j++) {
            if (j == 19)
                fprintf(pFile, "%#X }, ", /* AR[] */
                        pViaModeTable->Modes[i].stdVgaTable.AR[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].stdVgaTable.AR[j]);
        }
        fprintf(pFile, "{ "); /* GR[] */
        for (j = 0; j < 9; j++) {
            if (j == 8)
                fprintf(pFile, "%#X } ", /* GR[] */
                        pViaModeTable->Modes[i].stdVgaTable.GR[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].stdVgaTable.GR[j]);
        }
        fprintf(pFile, "}, "); /* stdVgaTable */
        fprintf(pFile, "{ "); /* extModeExtTable */
        fprintf(pFile, "{ "); /* extModeExtTable.port[] */
        for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* extModeExtTable.port[] */
                        pViaModeTable->Modes[i].extModeExtTable.port[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].extModeExtTable.port[j]);
        }
        fprintf(pFile, "{ "); /* extModeExtTable.offset[] */
        for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* extModeExtTable.offset[] */
                        pViaModeTable->Modes[i].extModeExtTable.offset[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].extModeExtTable.offset[j]);
        }
        fprintf(pFile, "{ "); /* extModeExtTable.mask[] */
        for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* extModeExtTable.mask[] */
                        pViaModeTable->Modes[i].extModeExtTable.mask[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].extModeExtTable.mask[j]);
        }
        fprintf(pFile, "{ "); /* extModeExtTable.data[] */
        for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* extModeExtTable.data[] */
                        pViaModeTable->Modes[i].extModeExtTable.data[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->Modes[i].extModeExtTable.data[j]);
        }
        fprintf(pFile, "%d ",
                pViaModeTable->Modes[i].extModeExtTable.numEntry);
        fprintf(pFile, "} "); /* extModeExtTable */
        if (i == (pViaModeTable->NumModes - 1))
            fprintf(pFile, "}\n");
        else
            fprintf(pFile, "},\n");
    }
    fprintf(pFile, "};\n\n");

    fprintf(pFile, "static const VIABIOSRegTableRec commExtTable = {\n"); /* commExtTable */
    fprintf(pFile, "    { "); /* commExtTable.port[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* commExtTable.port[] */
                    pViaModeTable->commExtTable.port[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->commExtTable.port[j]);
    }
    fprintf(pFile, "{ "); /* commExtTable.offset[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* commExtTable.offset[] */
                    pViaModeTable->commExtTable.offset[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->commExtTable.offset[j]);
    }
    fprintf(pFile, "{ "); /* commExtTable.mask[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* commExtTable.mask[] */
                    pViaModeTable->commExtTable.mask[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->commExtTable.mask[j]);
    }
    fprintf(pFile, "{ "); /* commExtTable.data[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* commExtTable.data[] */
                    pViaModeTable->commExtTable.data[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->commExtTable.data[j]);
    }
    fprintf(pFile, "%d ",
            pViaModeTable->commExtTable.numEntry);
    fprintf(pFile, "};\n\n"); /* commExtTable */

    fprintf(pFile, "static const VIABIOSRegTableRec stdModeExtTable = {\n"); /* stdModeExtTable */
    fprintf(pFile, "    { "); /* stdModeExtTable.port[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* stdModeExtTable.port[] */
                    pViaModeTable->stdModeExtTable.port[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->stdModeExtTable.port[j]);
    }
    fprintf(pFile, "{ "); /* stdModeExtTable.offset[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* stdModeExtTable.offset[] */
                    pViaModeTable->stdModeExtTable.offset[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->stdModeExtTable.offset[j]);
    }
    fprintf(pFile, "{ "); /* stdModeExtTable.mask[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* stdModeExtTable.mask[] */
                    pViaModeTable->stdModeExtTable.mask[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->stdModeExtTable.mask[j]);
    }
    fprintf(pFile, "{ "); /* stdModeExtTable.data[] */
    for (j = 0; j < VIA_BIOS_REG_TABLE_MAX_NUM; j++) {
        if (j == (VIA_BIOS_REG_TABLE_MAX_NUM - 1))
            fprintf(pFile, "%#X }, ", /* stdModeExtTable.data[] */
                    pViaModeTable->stdModeExtTable.data[j]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->stdModeExtTable.data[j]);
    }
    fprintf(pFile, "%d ",
            pViaModeTable->stdModeExtTable.numEntry);
    fprintf(pFile, "};\n\n"); /* stdModeExtTable */

    fprintf(pFile, "static const VIABIOSRefreshTableRec refreshTable[%d][%d] = {\n",
            VIA_BIOS_NUM_RES, VIA_BIOS_NUM_REFRESH); /* refreshTable */
    for (i = 0; i < VIA_BIOS_NUM_RES; i++) {
        fprintf(pFile, "    {\n");
        for (j = 0; j < VIA_BIOS_NUM_REFRESH; j++) {
            fprintf(pFile, "        { %d, %#X, ",
                    pViaModeTable->refreshTable[i][j].refresh,
                    pViaModeTable->refreshTable[i][j].VClk);
            fprintf(pFile, "{ "); /* refreshTable.CR[] */
            for (k = 0; k < 14; k++) {
                if (k == 13)
                    fprintf(pFile, "%#X } ", /* refreshTable.CR[] */
                            pViaModeTable->refreshTable[i][j].CR[k]);
                else
                    fprintf(pFile, "%#X, ",
                            pViaModeTable->refreshTable[i][j].CR[k]);
            }
            if (j == (VIA_BIOS_NUM_REFRESH - 1)) {
                if (i == (VIA_BIOS_NUM_RES - 1))
                    fprintf(pFile, "}\n");
                else
                    fprintf(pFile, "},\n");
            }
            else
                fprintf(pFile, "},\n");
        }
        if (i == (VIA_BIOS_NUM_RES - 1)) {
            fprintf(pFile, "    }\n");
            fprintf(pFile, "};\n\n");
        }
        else
            fprintf(pFile, "    },\n");
    }

    fprintf(pFile, "static const VIALCDModeTableRec lcdTable[] = {\n"); /* lcdTable */
    for (i = 0; i < VIA_BIOS_NUM_PANEL; i++) {
        fprintf(pFile, "    { %#X, %#X, %#X, %d, %d, %d, %d, \n",
                pViaModeTable->lcdTable[i].fpIndex,
                pViaModeTable->lcdTable[i].fpSize,
                pViaModeTable->lcdTable[i].powerSeq,
                pViaModeTable->lcdTable[i].numMPatchDP2Ctr,
                pViaModeTable->lcdTable[i].numMPatchDP2Exp,
                pViaModeTable->lcdTable[i].numMPatchDP1Ctr,
                pViaModeTable->lcdTable[i].numMPatchDP1Exp);

        fprintf(pFile, "        { "); /* SuptMode */
        for (j = 0; j < VIA_BIOS_NUM_LCD_SUPPORT_MASK; j++) {
            if (j == (VIA_BIOS_NUM_LCD_SUPPORT_MASK - 1))
                fprintf(pFile, "%#X }, \n",  /* SuptMode */
                        pViaModeTable->lcdTable[i].SuptMode[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].SuptMode[j]);
        }

        fprintf(pFile, "        { %#X, %#X, %#X, %#X, ", /* FPconfigTb */
                pViaModeTable->lcdTable[i].FPconfigTb.LCDClk,
                pViaModeTable->lcdTable[i].FPconfigTb.VClk,
                pViaModeTable->lcdTable[i].FPconfigTb.LCDClk_12Bit,
                pViaModeTable->lcdTable[i].FPconfigTb.VClk_12Bit);
        fprintf(pFile, "{ "); /* FPconfigTb.port[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ",  /* FPconfigTb.port[] */
                        pViaModeTable->lcdTable[i].FPconfigTb.port[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].FPconfigTb.port[j]);
        }
        fprintf(pFile, "{ "); /* FPconfigTb.offset[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* FPconfigTb.offset[] */
                        pViaModeTable->lcdTable[i].FPconfigTb.offset[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].FPconfigTb.offset[j]);
        }
        fprintf(pFile, "{ "); /* FPconfigTb.data[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* FPconfigTb.data[] */
                        pViaModeTable->lcdTable[i].FPconfigTb.data[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].FPconfigTb.data[j]);
        }
        fprintf(pFile, "%d }, \n", /* FPconfigTb */
                pViaModeTable->lcdTable[i].FPconfigTb.numEntry);

        fprintf(pFile, "        { %#X, %#X, %#X, %#X, ", /* InitTb */
                pViaModeTable->lcdTable[i].InitTb.LCDClk,
                pViaModeTable->lcdTable[i].InitTb.VClk,
                pViaModeTable->lcdTable[i].InitTb.LCDClk_12Bit,
                pViaModeTable->lcdTable[i].InitTb.VClk_12Bit);
        fprintf(pFile, "{ "); /* InitTb.port[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ",  /* InitTb.port[] */
                        pViaModeTable->lcdTable[i].InitTb.port[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].InitTb.port[j]);
        }
        fprintf(pFile, "{ "); /* InitTb.offset[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* InitTb.offset[] */
                        pViaModeTable->lcdTable[i].InitTb.offset[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].InitTb.offset[j]);
        }
        fprintf(pFile, "{ "); /* InitTb.data[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* InitTb.data[] */
                        pViaModeTable->lcdTable[i].InitTb.data[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].InitTb.data[j]);
        }
        fprintf(pFile, "%d }, \n", /* InitTb */
                pViaModeTable->lcdTable[i].InitTb.numEntry);

        /* MPatch Table */
        for (m = 0; m < 4; m++) {
            switch (m) {
                case 0: /* MPatchDP2Ctr */
                    MPatch = pViaModeTable->lcdTable[i].MPatchDP2Ctr;
                    numMPatch = VIA_BIOS_MAX_NUM_MPATCH2;
                    break;
                case 1: /* MPatchDP2Exp */
                    MPatch = pViaModeTable->lcdTable[i].MPatchDP2Exp;
                    numMPatch = VIA_BIOS_MAX_NUM_MPATCH2;
                    break;
                case 2: /* MPatchDP1Ctr */
                    MPatch = pViaModeTable->lcdTable[i].MPatchDP1Ctr;
                    numMPatch = VIA_BIOS_MAX_NUM_MPATCH1;
                    break;
                case 3: /* MPatchDP1Exp */
                    MPatch = pViaModeTable->lcdTable[i].MPatchDP1Exp;
                    numMPatch = VIA_BIOS_MAX_NUM_MPATCH1;
                    break;
            }

            fprintf(pFile, "        { \n");
            for (k = 0; k < numMPatch; k++) {
                fprintf(pFile, "            { %#X, %#X, %#X, %#X, %#X, ",
                        MPatch[k].Mode,
                        MPatch[k].LCDClk,
                        MPatch[k].VClk,
                        MPatch[k].LCDClk_12Bit,
                        MPatch[k].VClk_12Bit);
            fprintf(pFile, "{ "); /* port[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ",  /* port[] */
                                MPatch[k].port[j]);
                    else
                        fprintf(pFile, "%#X, ",
                                MPatch[k].port[j]);
            }
            fprintf(pFile, "{ "); /* offset[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ", /* offset[] */
                                MPatch[k].offset[j]);
                    else
                        fprintf(pFile, "%#X, ",
                                MPatch[k].offset[j]);
            }
            fprintf(pFile, "{ "); /* data[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ", /* data[] */
                                MPatch[k].data[j]);
                    else
                        fprintf(pFile, "%#X, ",
                                MPatch[k].data[j]);
                }
                if (k == (numMPatch - 1))
                    fprintf(pFile, "%d }\n        }, \n",
                            MPatch[k].numEntry);
                else
                    fprintf(pFile, "%d }, \n",
                            MPatch[k].numEntry);
            }
        }

        fprintf(pFile, "        { %#X, %#X, %#X, %#X, ", /* LowResCtr */
                pViaModeTable->lcdTable[i].LowResCtr.LCDClk,
                pViaModeTable->lcdTable[i].LowResCtr.VClk,
                pViaModeTable->lcdTable[i].LowResCtr.LCDClk_12Bit,
                pViaModeTable->lcdTable[i].LowResCtr.VClk_12Bit);
        fprintf(pFile, "{ "); /* LowResCtr.port[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ",  /* LowResCtr.port[] */
                        pViaModeTable->lcdTable[i].LowResCtr.port[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].LowResCtr.port[j]);
        }
        fprintf(pFile, "{ "); /* LowResCtr.offset[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* LowResCtr.offset[] */
                        pViaModeTable->lcdTable[i].LowResCtr.offset[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].LowResCtr.offset[j]);
        }
        fprintf(pFile, "{ "); /* LowResCtr.data[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* LowResCtr.data[] */
                        pViaModeTable->lcdTable[i].LowResCtr.data[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].LowResCtr.data[j]);
        }
        fprintf(pFile, "%d }, \n", /* LowResCtr */
                pViaModeTable->lcdTable[i].LowResCtr.numEntry);

        fprintf(pFile, "        { %#X, %#X, %#X, %#X, ", /* LowResExp */
                pViaModeTable->lcdTable[i].LowResExp.LCDClk,
                pViaModeTable->lcdTable[i].LowResExp.VClk,
                pViaModeTable->lcdTable[i].LowResExp.LCDClk_12Bit,
                pViaModeTable->lcdTable[i].LowResExp.VClk_12Bit);
        fprintf(pFile, "{ "); /* LowResExp.port[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ",  /* LowResExp.port[] */
                        pViaModeTable->lcdTable[i].LowResExp.port[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].LowResExp.port[j]);
        }
        fprintf(pFile, "{ "); /* LowResExp.offset[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* LowResExp.offset[] */
                        pViaModeTable->lcdTable[i].LowResExp.offset[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].LowResExp.offset[j]);
        }
        fprintf(pFile, "{ "); /* LowResExp.data[] */
        for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
            if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                fprintf(pFile, "%#X }, ", /* LowResExp.data[] */
                        pViaModeTable->lcdTable[i].LowResExp.data[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->lcdTable[i].LowResExp.data[j]);
        }
        fprintf(pFile, "%d }, \n", /* LowResExp */
                pViaModeTable->lcdTable[i].LowResExp.numEntry);

        fprintf(pFile, "        { \n"); /* MCtr */
        for (k = 0; k < VIA_BIOS_MAX_NUM_CTREXP; k++) {
            fprintf(pFile, "            { %#X, %#X, %#X, %#X, ",
                    pViaModeTable->lcdTable[i].MCtr[k].LCDClk,
                    pViaModeTable->lcdTable[i].MCtr[k].VClk,
                    pViaModeTable->lcdTable[i].MCtr[k].LCDClk_12Bit,
                    pViaModeTable->lcdTable[i].MCtr[k].VClk_12Bit);
            fprintf(pFile, "{ "); /* port[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ",  /* port[] */
                            pViaModeTable->lcdTable[i].MCtr[k].port[j]);
                else
                    fprintf(pFile, "%#X, ",
                            pViaModeTable->lcdTable[i].MCtr[k].port[j]);
            }
            fprintf(pFile, "{ "); /* offset[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ", /* offset[] */
                            pViaModeTable->lcdTable[i].MCtr[k].offset[j]);
                else
                    fprintf(pFile, "%#X, ",
                            pViaModeTable->lcdTable[i].MCtr[k].offset[j]);
            }
            fprintf(pFile, "{ "); /* data[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ", /* data[] */
                            pViaModeTable->lcdTable[i].MCtr[k].data[j]);
                else
                    fprintf(pFile, "%#X, ",
                            pViaModeTable->lcdTable[i].MCtr[k].data[j]);
            }
            if (k == (VIA_BIOS_MAX_NUM_CTREXP - 1))
                fprintf(pFile, "%d }\n        }, \n",
                        pViaModeTable->lcdTable[i].MCtr[k].numEntry);
            else
                fprintf(pFile, "%d }, \n",
                        pViaModeTable->lcdTable[i].MCtr[k].numEntry);
        }

        fprintf(pFile, "        { \n"); /* MExp */
        for (k = 0; k < VIA_BIOS_MAX_NUM_CTREXP; k++) {
            fprintf(pFile, "            { %#X, %#X, %#X, %#X, ",
                    pViaModeTable->lcdTable[i].MExp[k].LCDClk,
                    pViaModeTable->lcdTable[i].MExp[k].VClk,
                    pViaModeTable->lcdTable[i].MExp[k].LCDClk_12Bit,
                    pViaModeTable->lcdTable[i].MExp[k].VClk_12Bit);
            fprintf(pFile, "{ "); /* port[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ",  /* port[] */
                            pViaModeTable->lcdTable[i].MExp[k].port[j]);
                else
                    fprintf(pFile, "%#X, ",
                            pViaModeTable->lcdTable[i].MExp[k].port[j]);
            }
            fprintf(pFile, "{ "); /* offset[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ", /* offset[] */
                            pViaModeTable->lcdTable[i].MExp[k].offset[j]);
                else
                    fprintf(pFile, "%#X, ",
                            pViaModeTable->lcdTable[i].MExp[k].offset[j]);
            }
            fprintf(pFile, "{ "); /* data[] */
            for (j = 0; j < VIA_BIOS_REG_LCD_MAX_NUM; j++) {
                if (j == (VIA_BIOS_REG_LCD_MAX_NUM - 1))
                    fprintf(pFile, "%#X }, ", /* data[] */
                            pViaModeTable->lcdTable[i].MExp[k].data[j]);
                else
                    fprintf(pFile, "%#X, ",
                            pViaModeTable->lcdTable[i].MExp[k].data[j]);
            }
            if (k == (VIA_BIOS_MAX_NUM_CTREXP - 1))
                fprintf(pFile, "%d }\n        }\n",
                        pViaModeTable->lcdTable[i].MExp[k].numEntry);
            else
                fprintf(pFile, "%d }, \n",
                        pViaModeTable->lcdTable[i].MExp[k].numEntry);
        }
        if (i == (VIA_BIOS_NUM_PANEL - 1))
            fprintf(pFile, "    }\n};\n\n");
        else
            fprintf(pFile, "    },\n");
    }

    /* powerOn */
    fprintf(pFile, "static const VIALCDPowerSeqRec powerOn[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_LCD_POWER_SEQ; i++) {
        fprintf(pFile, "    { %d, ",
                pViaModeTable->powerOn[i].powerSeq);
        fprintf(pFile, "{ "); /* port[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ",  /* port[] */
                        pViaModeTable->powerOn[i].port[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOn[i].port[j]);
        }
        fprintf(pFile, "{ "); /* offset[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* offset[] */
                        pViaModeTable->powerOn[i].offset[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOn[i].offset[j]);
        }
        fprintf(pFile, "{ "); /* mask[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* mask[] */
                        pViaModeTable->powerOn[i].mask[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOn[i].mask[j]);
        }
        fprintf(pFile, "{ "); /* data[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* data[] */
                        pViaModeTable->powerOn[i].data[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOn[i].data[j]);
        }
        fprintf(pFile, "{ "); /* delay[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* delay[] */
                        pViaModeTable->powerOn[i].delay[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOn[i].delay[j]);
        }
        if (i == (VIA_BIOS_NUM_LCD_POWER_SEQ - 1)) {
            fprintf(pFile, "%d }\n}; \n\n",
                    pViaModeTable->powerOn[i].numEntry);
        }
        else
            fprintf(pFile, "%d }, \n",
                    pViaModeTable->powerOn[i].numEntry);
    }

    /* powerOff */
    fprintf(pFile, "static const VIALCDPowerSeqRec powerOff[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_LCD_POWER_SEQ; i++) {
        fprintf(pFile, "    { %d, ",
                pViaModeTable->powerOff[i].powerSeq);
        fprintf(pFile, "{ "); /* port[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ",  /* port[] */
                        pViaModeTable->powerOff[i].port[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOff[i].port[j]);
        }
        fprintf(pFile, "{ "); /* offset[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* offset[] */
                        pViaModeTable->powerOff[i].offset[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOff[i].offset[j]);
        }
        fprintf(pFile, "{ "); /* mask[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* mask[] */
                        pViaModeTable->powerOff[i].mask[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOff[i].mask[j]);
        }
        fprintf(pFile, "{ "); /* data[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* data[] */
                        pViaModeTable->powerOff[i].data[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOff[i].data[j]);
        }
        fprintf(pFile, "{ "); /* delay[] */
        for (j = 0; j < 4; j++) {
            if (j == 3)
                fprintf(pFile, "%#X }, ", /* delay[] */
                        pViaModeTable->powerOff[i].delay[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->powerOff[i].delay[j]);
        }
        if (i == (VIA_BIOS_NUM_LCD_POWER_SEQ - 1)) {
            fprintf(pFile, "%d }\n}; \n\n",
                    pViaModeTable->powerOff[i].numEntry);
        }
        else
            fprintf(pFile, "%d }, \n",
                    pViaModeTable->powerOff[i].numEntry);
    }

    /* ModeFix */
    fprintf(pFile, "static const VIALCDModeFixRec modeFix = {\n");
    fprintf(pFile, "    { "); /* modeFix.reqMode[] */
    for (i = 0; i < 32; i++) {
        if (i == 31)
            fprintf(pFile, "%#X }, ",  /* ModeFix.reqMode[] */
                    pViaModeTable->modeFix.reqMode[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->modeFix.reqMode[i]);
    }
    fprintf(pFile, "{ "); /* modeFix.fixMode[] */
    for (i = 0; i < 32; i++) {
        if (i == 31)
            fprintf(pFile, "%#X }, ",  /* modeFix.fixMode[] */
                    pViaModeTable->modeFix.fixMode[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->modeFix.fixMode[i]);
    }
    fprintf(pFile, "%d }; \n\n",
            pViaModeTable->modeFix.numEntry);

    fprintf(pFile, "\n");
    fprintf(pFile, "#endif\n");

    if (fclose(pFile) != 0) {
        ErrorF("Error closing file!!\n");
        return FALSE;
    }
    else
        return TRUE;
}
#endif /* CREATE_MODETABLE_HEADERFILE */

#ifdef CREATE_FS454_HEADERFILE
Bool VIACreateFS454(VIAModeTablePtr pViaModeTable)
{
    int             i, j;
    FILE            *pFile;

    if ((pFile = fopen("via_fs454.h", "w+")) == NULL) {
        ErrorF("Can't open \"via_fs454.h\" file!!\n");
        return FALSE;
    }

    fprintf(pFile, "#ifndef _VIA_FS454MODETABLE_H\n");
    fprintf(pFile, "#define _VIA_FS454MODETABLE_H\n");
    fprintf(pFile, "\n");

    /* fs454MaskTable */
    fprintf(pFile, "static const VIABIOSFS454TVMASKTableRec fs454MaskTable = {\n");
    fprintf(pFile, "    { ");
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* fs454MaskTable.CRTC1[] */
                    pViaModeTable->fs454MaskTable.CRTC1[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->fs454MaskTable.CRTC1[i]);
    }

    fprintf(pFile, "    { "); /* fs454MaskTable.CRTC2[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* fs454MaskTable.CRTC2[] */
                    pViaModeTable->fs454MaskTable.CRTC2[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->fs454MaskTable.CRTC2[i]);
    }

    fprintf(pFile, "    %#X, %#X, %d, %d, %d \n};\n\n",
            pViaModeTable->fs454MaskTable.misc1,
            pViaModeTable->fs454MaskTable.misc2,
            pViaModeTable->fs454MaskTable.numCRTC1,
            pViaModeTable->fs454MaskTable.numCRTC2);

    /* fs454Table */
    fprintf(pFile, "static const VIABIOSFS454TableRec fs454Table[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_FS454; i++) {
        fprintf(pFile, "    {\n");

        fprintf(pFile, "        { "); /* fs454Table.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_FS454_TV_REG; j++) {
            if (j == (VIA_BIOS_NUM_FS454_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.TVNTSC[] */
                        pViaModeTable->fs454Table[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.CRTCNTSC1[] */
                        pViaModeTable->fs454Table[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.MiscNTSC1[] */
                        pViaModeTable->fs454Table[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.MiscNTSC2[] */
                        pViaModeTable->fs454Table[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.CRTCNTSC2_8BPP[] */
                        pViaModeTable->fs454Table[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.CRTCNTSC2_16BPP[] */
                        pViaModeTable->fs454Table[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.CRTCNTSC2_32BPP[] */
                        pViaModeTable->fs454Table[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.RGBNTSC[] */
                        pViaModeTable->fs454Table[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.YCbCrNTSC[] */
                        pViaModeTable->fs454Table[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* fs454Table.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* fs454Table.DotCrawlNTSC[] */
                        pViaModeTable->fs454Table[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454Table[i].DotCrawlNTSC[j]);
        }

        if (i == (VIA_BIOS_NUM_FS454 - 1))
            fprintf(pFile, "    }\n};\n\n");
        else
            fprintf(pFile, "    }, \n");
    }

    /* fs454OverTable */
    fprintf(pFile, "static const VIABIOSFS454TableRec fs454OverTable[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_FS454; i++) {
        fprintf(pFile, "    {\n");

        fprintf(pFile, "        { ");
        for (j = 0; j < VIA_BIOS_NUM_FS454_TV_REG; j++) {
            if (j == (VIA_BIOS_NUM_FS454_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.TVNTSC[] */
                        pViaModeTable->fs454OverTable[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.CRTCNTSC1[] */
                        pViaModeTable->fs454OverTable[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.MiscNTSC1[] */
                        pViaModeTable->fs454OverTable[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.MiscNTSC2[] */
                        pViaModeTable->fs454OverTable[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.CRTCNTSC2_8BPP[] */
                        pViaModeTable->fs454OverTable[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.CRTCNTSC2_16BPP[] */
                        pViaModeTable->fs454OverTable[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.CRTCNTSC2_32BPP[] */
                        pViaModeTable->fs454OverTable[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.RGBNTSC[] */
                        pViaModeTable->fs454OverTable[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.YCbCrNTSC[] */
                        pViaModeTable->fs454OverTable[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* fs454OverTable.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* fs454OverTable.DotCrawlNTSC[] */
                        pViaModeTable->fs454OverTable[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->fs454OverTable[i].DotCrawlNTSC[j]);
        }

        if (i == (VIA_BIOS_NUM_FS454 - 1))
            fprintf(pFile, "    }\n};\n");
        else
            fprintf(pFile, "    }, \n");
    }

    fprintf(pFile, "\n");
    fprintf(pFile, "#endif\n");

    return TRUE;
}
#endif /* CREATE_FS454_HEADERFILE */

#ifdef CREATE_TV2_HEADERFILE
Bool VIACreateTV2(VIAModeTablePtr pViaModeTable)
{
    int             i, j;
    FILE            *pFile;


    if ((pFile = fopen("via_tv2.h", "w+")) == NULL) {
        ErrorF("Can't open \"via_tv2.h\" file!!\n");
        return FALSE;
    }

    fprintf(pFile, "#ifndef _VIA_TV2MODETABLE_H\n");
    fprintf(pFile, "#define _VIA_TV2MODETABLE_H\n");
    fprintf(pFile, "\n");

    /* tv2MaskTable */
    fprintf(pFile, "static const VIABIOSTVMASKTableRec tv2MaskTable = {\n");
    fprintf(pFile, "    { "); /* tv2MaskTable.TV[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_REG - 1))
            fprintf(pFile, "%#X },\n", /* tv2MaskTable.TV[] */
                    pViaModeTable->tv2MaskTable.TV[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->tv2MaskTable.TV[i]);
    }

    fprintf(pFile, "    { "); /* tv2MaskTable.CRTC1[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* tv2MaskTable.CRTC1[] */
                    pViaModeTable->tv2MaskTable.CRTC1[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->tv2MaskTable.CRTC1[i]);
    }

    fprintf(pFile, "    { "); /* tv2MaskTable.CRTC2[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* tv2MaskTable.CRTC2[] */
                    pViaModeTable->tv2MaskTable.CRTC2[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->tv2MaskTable.CRTC2[i]);
    }

    fprintf(pFile, "    %#X, %#X, %d, %d, %d \n};\n\n",
            pViaModeTable->tv2MaskTable.misc1,
            pViaModeTable->tv2MaskTable.misc2,
            pViaModeTable->tv2MaskTable.numTV,
            pViaModeTable->tv2MaskTable.numCRTC1,
            pViaModeTable->tv2MaskTable.numCRTC2);

    fprintf(pFile, "static const VIABIOSTV2TableRec tv2Table[] = {\n"); /* tv2Table */
    for (i = 0; i < VIA_BIOS_NUM_TV2; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* tv2Table.TVNTSCC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.TVNTSCC[] */
                        pViaModeTable->tv2Table[i].TVNTSCC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].TVNTSCC[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.TVNTSCS[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.TVNTSCS[] */
                        pViaModeTable->tv2Table[i].TVNTSCS[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].TVNTSCS[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCNTSC1[] */
                        pViaModeTable->tv2Table[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.MiscNTSC1[] */
                        pViaModeTable->tv2Table[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.MiscNTSC2[] */
                        pViaModeTable->tv2Table[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCNTSC2_8BPP[] */
                        pViaModeTable->tv2Table[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCNTSC2_16BPP[] */
                        pViaModeTable->tv2Table[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCNTSC2_32BPP[] */
                        pViaModeTable->tv2Table[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.PatchNTSC[] */
                        pViaModeTable->tv2Table[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.DotCrawlNTSC[] */
                        pViaModeTable->tv2Table[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.TVPALC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.TVPALC[] */
                        pViaModeTable->tv2Table[i].TVPALC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].TVPALC[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.TVPALS[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.TVPALS[] */
                        pViaModeTable->tv2Table[i].TVPALS[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].TVPALS[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCPAL1[] */
                        pViaModeTable->tv2Table[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.MiscPAL1[] */
                        pViaModeTable->tv2Table[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.MiscPAL2[] */
                        pViaModeTable->tv2Table[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCPAL2_8BPP[] */
                        pViaModeTable->tv2Table[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCPAL2_16BPP[] */
                        pViaModeTable->tv2Table[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2Table.CRTCPAL2_32BPP[] */
                        pViaModeTable->tv2Table[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X }\n", /* tv2Table.PatchPAL2[] */
                        pViaModeTable->tv2Table[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2Table[i].PatchPAL2[j]);
        }

        if (i == (VIA_BIOS_NUM_TV2 - 1))
            fprintf(pFile, "    }\n};\n\n");
        else
            fprintf(pFile, "    }, \n");
    }

    /* tv2OverTable */
    fprintf(pFile, "static const VIABIOSTV2TableRec tv2OverTable[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_TV2; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* tv2OverTable.TVNTSCC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.TVNTSCC[] */
                        pViaModeTable->tv2OverTable[i].TVNTSCC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].TVNTSCC[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.TVNTSCS[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.TVNTSCS[] */
                        pViaModeTable->tv2OverTable[i].TVNTSCS[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].TVNTSCS[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCNTSC1[] */
                        pViaModeTable->tv2OverTable[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.MiscNTSC1[] */
                        pViaModeTable->tv2OverTable[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.MiscNTSC2[] */
                        pViaModeTable->tv2OverTable[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCNTSC2_8BPP[] */
                        pViaModeTable->tv2OverTable[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCNTSC2_16BPP[] */
                        pViaModeTable->tv2OverTable[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCNTSC2_32BPP[] */
                        pViaModeTable->tv2OverTable[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.PatchNTSC[] */
                        pViaModeTable->tv2OverTable[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.DotCrawlNTSC[] */
                        pViaModeTable->tv2OverTable[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.TVPALC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.TVPALC[] */
                        pViaModeTable->tv2OverTable[i].TVPALC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].TVPALC[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.TVPALS[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.TVPALS[] */
                        pViaModeTable->tv2OverTable[i].TVPALS[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].TVPALS[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCPAL1[] */
                        pViaModeTable->tv2OverTable[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.MiscPAL1[] */
                        pViaModeTable->tv2OverTable[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.MiscPAL2[] */
                        pViaModeTable->tv2OverTable[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCPAL2_8BPP[] */
                        pViaModeTable->tv2OverTable[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCPAL2_16BPP[] */
                        pViaModeTable->tv2OverTable[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2OverTable.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv2OverTable.CRTCPAL2_32BPP[] */
                        pViaModeTable->tv2OverTable[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv2Table.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X }\n", /* tv2Table.PatchPAL2[] */
                        pViaModeTable->tv2OverTable[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv2OverTable[i].PatchPAL2[j]);
        }

        if (i == (VIA_BIOS_NUM_TV2 - 1))
            fprintf(pFile, "    }\n};\n");
        else
            fprintf(pFile, "    }, \n");
    }

    fprintf(pFile, "\n");
    fprintf(pFile, "#endif\n");

    return TRUE;
}
#endif /* CREATE_TV2_HEADERFILE */
#ifdef CREATE_TV3_HEADERFILE
Bool VIACreateTV3(VIAModeTablePtr pViaModeTable)
{
    int             i, j;
    FILE            *pFile;

    if ((pFile = fopen("via_tv3.h", "w+")) == NULL) {
        ErrorF("Can't open \"via_tv3.h\" file!!\n");
        return FALSE;
    }

    fprintf(pFile, "#ifndef _VIA_TV3MODETABLE_H\n");
    fprintf(pFile, "#define _VIA_TV3MODETABLE_H\n");
    fprintf(pFile, "\n");

    /* tv3MaskTable */
    fprintf(pFile, "static const VIABIOSTVMASKTableRec tv3MaskTable = {\n");
    fprintf(pFile, "    { "); /* tv3MaskTable.TV[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_REG - 1))
            fprintf(pFile, "%#X },\n", /* tv3MaskTable.TV[] */
                    pViaModeTable->tv3MaskTable.TV[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->tv3MaskTable.TV[i]);
    }

    fprintf(pFile, "    { "); /* tv3MaskTable.CRTC1[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* tv3MaskTable.CRTC1[] */
                    pViaModeTable->tv3MaskTable.CRTC1[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->tv3MaskTable.CRTC1[i]);
    }

    fprintf(pFile, "    { "); /* tv3MaskTable.CRTC2[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* tv3MaskTable.CRTC2[] */
                    pViaModeTable->tv3MaskTable.CRTC2[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->tv3MaskTable.CRTC2[i]);
    }

    fprintf(pFile, "    %#X, %#X, %d, %d, %d \n};\n\n",
            pViaModeTable->tv3MaskTable.misc1,
            pViaModeTable->tv3MaskTable.misc2,
            pViaModeTable->tv3MaskTable.numTV,
            pViaModeTable->tv3MaskTable.numCRTC1,
            pViaModeTable->tv3MaskTable.numCRTC2);

    /* tv3Table */
    fprintf(pFile, "static const VIABIOSTV3TableRec tv3Table[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_TV3; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* tv3Table.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.TVNTSC[] */
                        pViaModeTable->tv3Table[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCNTSC1[] */
                        pViaModeTable->tv3Table[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.MiscNTSC1[] */
                        pViaModeTable->tv3Table[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.MiscNTSC2[] */
                        pViaModeTable->tv3Table[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCNTSC2_8BPP[] */
                        pViaModeTable->tv3Table[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCNTSC2_16BPP[] */
                        pViaModeTable->tv3Table[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCNTSC2_32BPP[] */
                        pViaModeTable->tv3Table[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.PatchNTSC2[] */
                        pViaModeTable->tv3Table[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.RGBNTSC[] */
                        pViaModeTable->tv3Table[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.YCbCrNTSC[] */
                        pViaModeTable->tv3Table[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.SDTV_RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.SDTV_RGBNTSC[] */
                        pViaModeTable->tv3Table[i].SDTV_RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].SDTV_RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.SDTV_YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.SDTV_YCbCrNTSC[] */
                        pViaModeTable->tv3Table[i].SDTV_YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].SDTV_YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.DotCrawlNTSC[] */
                        pViaModeTable->tv3Table[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.TVPAL[] */
                        pViaModeTable->tv3Table[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCPAL1[] */
                        pViaModeTable->tv3Table[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.MiscPAL1[] */
                        pViaModeTable->tv3Table[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.MiscPAL2[] */
                        pViaModeTable->tv3Table[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCPAL2_8BPP[] */
                        pViaModeTable->tv3Table[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCPAL2_16BPP[] */
                        pViaModeTable->tv3Table[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.CRTCPAL2_32BPP[] */
                        pViaModeTable->tv3Table[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.PatchPAL2[] */
                        pViaModeTable->tv3Table[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].PatchPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.RGBPAL[] */
                        pViaModeTable->tv3Table[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.YCbCrPAL[] */
                        pViaModeTable->tv3Table[i].YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].YCbCrPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.SDTV_RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3Table.SDTV_RGBPAL[] */
                        pViaModeTable->tv3Table[i].SDTV_RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].SDTV_RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3Table.SDTV_YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X }\n", /* tv3Table.SDTV_YCbCrPAL[] */
                        pViaModeTable->tv3Table[i].SDTV_YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3Table[i].SDTV_YCbCrPAL[j]);
        }

        if (i == (VIA_BIOS_NUM_TV3 - 1))
            fprintf(pFile, "    }\n};\n\n");
        else
            fprintf(pFile, "    }, \n");
    }

    /* tv3OverTable */
    fprintf(pFile, "static const VIABIOSTV3TableRec tv3OverTable[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_TV3; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* tv3OverTable.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.TVNTSC[] */
                        pViaModeTable->tv3OverTable[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCNTSC1[] */
                        pViaModeTable->tv3OverTable[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.MiscNTSC1[] */
                        pViaModeTable->tv3OverTable[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.MiscNTSC2[] */
                        pViaModeTable->tv3OverTable[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCNTSC2_8BPP[] */
                        pViaModeTable->tv3OverTable[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCNTSC2_16BPP[] */
                        pViaModeTable->tv3OverTable[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCNTSC2_32BPP[] */
                        pViaModeTable->tv3OverTable[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.PatchNTSC2[] */
                        pViaModeTable->tv3OverTable[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.RGBNTSC[] */
                        pViaModeTable->tv3OverTable[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.YCbCrNTSC[] */
                        pViaModeTable->tv3OverTable[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.SDTV_RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.SDTV_RGBNTSC[] */
                        pViaModeTable->tv3OverTable[i].SDTV_RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].SDTV_RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.SDTV_YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.SDTV_YCbCrNTSC[] */
                        pViaModeTable->tv3OverTable[i].SDTV_YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].SDTV_YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.DotCrawlNTSC[] */
                        pViaModeTable->tv3OverTable[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.TVPAL[] */
                        pViaModeTable->tv3OverTable[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCPAL1[] */
                        pViaModeTable->tv3OverTable[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.MiscPAL1[] */
                        pViaModeTable->tv3OverTable[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.MiscPAL2[] */
                        pViaModeTable->tv3OverTable[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv3Over.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCPAL2_8BPP[] */
                        pViaModeTable->tv3OverTable[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCPAL2_16BPP[] */
                        pViaModeTable->tv3OverTable[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.CRTCPAL2_32BPP[] */
                        pViaModeTable->tv3OverTable[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.PatchPAL2[] */
                        pViaModeTable->tv3OverTable[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].PatchPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.RGBPAL[] */
                        pViaModeTable->tv3OverTable[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.YCbCrPAL[] */
                        pViaModeTable->tv3OverTable[i].YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].YCbCrPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.SDTV_RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* tv3OverTable.SDTV_RGBPAL[] */
                        pViaModeTable->tv3OverTable[i].SDTV_RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].SDTV_RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* tv3OverTable.SDTV_YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X }\n", /* tv3OverTable.SDTV_YCbCrPAL[] */
                        pViaModeTable->tv3OverTable[i].SDTV_YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->tv3OverTable[i].SDTV_YCbCrPAL[j]);
        }

        if (i == (VIA_BIOS_NUM_TV3 - 1))
            fprintf(pFile, "    }\n};\n");
        else
            fprintf(pFile, "    }, \n");
    }

    fprintf(pFile, "\n");
    fprintf(pFile, "#endif\n");

    return TRUE;
}
#endif /* CREATE_TV3_HEADERFILE */

#ifdef CREATE_VT1622A_HEADERFILE
Bool VIACreateVT1622A(VIAModeTablePtr pViaModeTable)
{
    int             i, j;
    FILE            *pFile;

    if ((pFile = fopen("via_vt1622a.h", "w+")) == NULL) {
        ErrorF("Can't open \"via_vt1622a.h\" file!!\n");
        return FALSE;
    }

    fprintf(pFile, "#ifndef _VIA_VT1622AMODETABLE_H\n");
    fprintf(pFile, "#define _VIA_VT1622AMODETABLE_H\n");
    fprintf(pFile, "\n");

    /* tv3MaskTable */
    fprintf(pFile, "static const VIABIOSTVMASKTableRec vt1622aMaskTable = {\n");
    fprintf(pFile, "    { "); /* vt1622aMaskTable.TV[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_REG - 1))
            fprintf(pFile, "%#X },\n", /* vt1622aMaskTable.TV[] */
                    pViaModeTable->vt1622aMaskTable.TV[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->vt1622aMaskTable.TV[i]);
    }

    fprintf(pFile, "    { "); /* vt1622aMaskTable.CRTC1[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* vt1622aMaskTable.CRTC1[] */
                    pViaModeTable->vt1622aMaskTable.CRTC1[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->vt1622aMaskTable.CRTC1[i]);
    }

    fprintf(pFile, "    { "); /* vt1622aMaskTable.CRTC2[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* vt1622aMaskTable.CRTC2[] */
                    pViaModeTable->vt1622aMaskTable.CRTC2[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->vt1622aMaskTable.CRTC2[i]);
    }

    fprintf(pFile, "    %#X, %#X, %d, %d, %d \n};\n\n",
            pViaModeTable->vt1622aMaskTable.misc1,
            pViaModeTable->vt1622aMaskTable.misc2,
            pViaModeTable->vt1622aMaskTable.numTV,
            pViaModeTable->vt1622aMaskTable.numCRTC1,
            pViaModeTable->vt1622aMaskTable.numCRTC2);

    /* tv3Table */
    fprintf(pFile, "static const VIABIOSTV3TableRec vt1622aTable[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_TV3; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* vt1622aTable.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.TVNTSC[] */
                        pViaModeTable->vt1622aTable[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCNTSC1[] */
                        pViaModeTable->vt1622aTable[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.MiscNTSC1[] */
                        pViaModeTable->vt1622aTable[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.MiscNTSC2[] */
                        pViaModeTable->vt1622aTable[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCNTSC2_8BPP[] */
                        pViaModeTable->vt1622aTable[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCNTSC2_16BPP[] */
                        pViaModeTable->vt1622aTable[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCNTSC2_32BPP[] */
                        pViaModeTable->vt1622aTable[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.PatchNTSC2[] */
                        pViaModeTable->vt1622aTable[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.RGBNTSC[] */
                        pViaModeTable->vt1622aTable[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.YCbCrNTSC[] */
                        pViaModeTable->vt1622aTable[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.SDTV_RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.SDTV_RGBNTSC[] */
                        pViaModeTable->vt1622aTable[i].SDTV_RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].SDTV_RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.SDTV_YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.SDTV_YCbCrNTSC[] */
                        pViaModeTable->vt1622aTable[i].SDTV_YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].SDTV_YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.DotCrawlNTSC[] */
                        pViaModeTable->vt1622aTable[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.TVPAL[] */
                        pViaModeTable->vt1622aTable[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCPAL1[] */
                        pViaModeTable->vt1622aTable[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.MiscPAL1[] */
                        pViaModeTable->vt1622aTable[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.MiscPAL2[] */
                        pViaModeTable->vt1622aTable[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCPAL2_8BPP[] */
                        pViaModeTable->vt1622aTable[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCPAL2_16BPP[] */
                        pViaModeTable->vt1622aTable[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.CRTCPAL2_32BPP[] */
                        pViaModeTable->vt1622aTable[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.PatchPAL2[] */
                        pViaModeTable->vt1622aTable[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].PatchPAL2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.RGBPAL[] */
                        pViaModeTable->vt1622aTable[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.YCbCrPAL[] */
                        pViaModeTable->vt1622aTable[i].YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].YCbCrPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.SDTV_RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aTable.SDTV_RGBPAL[] */
                        pViaModeTable->vt1622aTable[i].SDTV_RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].SDTV_RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aTable.SDTV_YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X }\n", /* vt1622aTable.SDTV_YCbCrPAL[] */
                        pViaModeTable->vt1622aTable[i].SDTV_YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aTable[i].SDTV_YCbCrPAL[j]);
        }

        if (i == (VIA_BIOS_NUM_TV3 - 1))
            fprintf(pFile, "    }\n};\n\n");
        else
            fprintf(pFile, "    }, \n");
    }

    /* tv3OverTable */
    fprintf(pFile, "static const VIABIOSTV3TableRec vt1622aOverTable[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_TV3; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* vt1622aOverTable.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.TVNTSC[] */
                        pViaModeTable->vt1622aOverTable[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCNTSC1[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.MiscNTSC1[] */
                        pViaModeTable->vt1622aOverTable[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.MiscNTSC2[] */
                        pViaModeTable->vt1622aOverTable[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCNTSC2_8BPP[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCNTSC2_16BPP[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCNTSC2_32BPP[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.PatchNTSC2[] */
                        pViaModeTable->vt1622aOverTable[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.RGBNTSC[] */
                        pViaModeTable->vt1622aOverTable[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.YCbCrNTSC[] */
                        pViaModeTable->vt1622aOverTable[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.SDTV_RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.SDTV_RGBNTSC[] */
                        pViaModeTable->vt1622aOverTable[i].SDTV_RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].SDTV_RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.SDTV_YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.SDTV_YCbCrNTSC[] */
                        pViaModeTable->vt1622aOverTable[i].SDTV_YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].SDTV_YCbCrNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.DotCrawlNTSC[] */
                        pViaModeTable->vt1622aOverTable[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.TVPAL[] */
                        pViaModeTable->vt1622aOverTable[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCPAL1[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.MiscPAL1[] */
                        pViaModeTable->vt1622aOverTable[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.MiscPAL2[] */
                        pViaModeTable->vt1622aOverTable[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOver.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCPAL2_8BPP[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCPAL2_16BPP[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.CRTCPAL2_32BPP[] */
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.PatchPAL2[] */
                        pViaModeTable->vt1622aOverTable[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].PatchPAL2[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.RGBPAL[] */
                        pViaModeTable->vt1622aOverTable[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.YCbCrPAL[] */
                        pViaModeTable->vt1622aOverTable[i].YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].YCbCrPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.SDTV_RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* vt1622aOverTable.SDTV_RGBPAL[] */
                        pViaModeTable->vt1622aOverTable[i].SDTV_RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].SDTV_RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* vt1622aOverTable.SDTV_YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X }\n", /* vt1622aOverTable.SDTV_YCbCrPAL[] */
                        pViaModeTable->vt1622aOverTable[i].SDTV_YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->vt1622aOverTable[i].SDTV_YCbCrPAL[j]);
        }

        if (i == (VIA_BIOS_NUM_TV3 - 1))
            fprintf(pFile, "    }\n};\n");
        else
            fprintf(pFile, "    }, \n");
    }

    fprintf(pFile, "\n");
    fprintf(pFile, "#endif\n");

    return TRUE;
}
#endif /* CREATE_VT1622A_HEADERFILE */

#ifdef CREATE_SAA7108_HEADERFILE
Bool VIACreateSAA7108(VIAModeTablePtr pViaModeTable)
{
    int             i, j;
    FILE            *pFile;

    if ((pFile = fopen("via_saa7108.h", "w+")) == NULL) {
        ErrorF("Can't open \"via_saa7108.h\" file!!\n");
        return FALSE;
    }

    fprintf(pFile, "#ifndef _VIA_SAA7108MODETABLE_H\n");
    fprintf(pFile, "#define _VIA_SAA7108MODETABLE_H\n");
    fprintf(pFile, "\n");

    /* saa7108MaskTable */
    fprintf(pFile, "static const VIABIOSSAA7108TVMASKTableRec saa7108MaskTable = {\n");
    fprintf(pFile, "    { "); /* saa7108MaskTable.TV[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_SAA7108_TV_REG; i++) {
        if (i == (VIA_BIOS_MAX_NUM_SAA7108_TV_REG - 1))
            fprintf(pFile, "%#X },\n", /* saa7108MaskTable.TV[] */
                    pViaModeTable->saa7108MaskTable.TV[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->saa7108MaskTable.TV[i]);
    }

    fprintf(pFile, "    { "); /* saa7108MaskTable.CRTC1[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* saa7108MaskTable.CRTC1[] */
                    pViaModeTable->saa7108MaskTable.CRTC1[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->saa7108MaskTable.CRTC1[i]);
    }

    fprintf(pFile, "    { "); /* saa7108MaskTable.CRTC2[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* saa7108MaskTable.CRTC2[] */
                    pViaModeTable->saa7108MaskTable.CRTC2[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->saa7108MaskTable.CRTC2[i]);
    }

    fprintf(pFile, "    %#X, %#X, %d, %d, %d \n};\n\n",
            pViaModeTable->saa7108MaskTable.misc1,
            pViaModeTable->saa7108MaskTable.misc2,
            pViaModeTable->saa7108MaskTable.numTV,
            pViaModeTable->saa7108MaskTable.numCRTC1,
            pViaModeTable->saa7108MaskTable.numCRTC2);

    /* saa7108Table */
    fprintf(pFile, "static const VIABIOSSAA7108TableRec saa7108Table[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_SAA7108; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* saa7108Table.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_SAA7108_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_SAA7108_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.TVNTSC[] */
                        pViaModeTable->saa7108Table[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCNTSC1[] */
                        pViaModeTable->saa7108Table[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.MiscNTSC1[] */
                        pViaModeTable->saa7108Table[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.MiscNTSC2[] */
                        pViaModeTable->saa7108Table[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCNTSC2_8BPP[] */
                        pViaModeTable->saa7108Table[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCNTSC2_16BPP[] */
                        pViaModeTable->saa7108Table[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCNTSC2_32BPP[] */
                        pViaModeTable->saa7108Table[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.PatchNTSC2[] */
                        pViaModeTable->saa7108Table[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.RGBNTSC[] */
                        pViaModeTable->saa7108Table[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.YCbCrNTSC[] */
                        pViaModeTable->saa7108Table[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].YCbCrNTSC[j]);
        }
#if 0
        fprintf(pFile, "        { "); /* saa7108Table.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.DotCrawlNTSC[] */
                        pViaModeTable->saa7108Table[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].DotCrawlNTSC[j]);
        }
#endif
        fprintf(pFile, "        { "); /* saa7108Table.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_SAA7108_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_SAA7108_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.TVPAL[] */
                        pViaModeTable->saa7108Table[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCPAL1[] */
                        pViaModeTable->saa7108Table[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.MiscPAL1[] */
                        pViaModeTable->saa7108Table[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.MiscPAL2[] */
                        pViaModeTable->saa7108Table[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCPAL2_8BPP[] */
                        pViaModeTable->saa7108Table[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCPAL2_16BPP[] */
                        pViaModeTable->saa7108Table[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.CRTCPAL2_32BPP[] */
                        pViaModeTable->saa7108Table[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.PatchPAL2[] */
                        pViaModeTable->saa7108Table[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].PatchPAL2[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.RGBPAL[] */
                        pViaModeTable->saa7108Table[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* saa7108Table.YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108Table.YCbCrPAL[] */
                        pViaModeTable->saa7108Table[i].YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108Table[i].YCbCrPAL[j]);
        }

        if (i == (VIA_BIOS_NUM_SAA7108 - 1))
            fprintf(pFile, "    }\n};\n\n");
        else
            fprintf(pFile, "    }, \n");
    }

    /* saa7108OverTable */
    fprintf(pFile, "static const VIABIOSSAA7108TableRec saa7108OverTable[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_SAA7108; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* saa7108OverTable.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_SAA7108_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_SAA7108_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.TVNTSC[] */
                        pViaModeTable->saa7108OverTable[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCNTSC1[] */
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.MiscNTSC1[] */
                        pViaModeTable->saa7108OverTable[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.MiscNTSC2[] */
                        pViaModeTable->saa7108OverTable[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCNTSC2_8BPP[] */
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCNTSC2_16BPP[] */
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCNTSC2_32BPP[] */
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.PatchNTSC2[] */
                        pViaModeTable->saa7108OverTable[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].PatchNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.RGBNTSC[] */
                        pViaModeTable->saa7108OverTable[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].RGBNTSC[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.YCbCrNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.YCbCrNTSC[] */
                        pViaModeTable->saa7108OverTable[i].YCbCrNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].YCbCrNTSC[j]);
        }
#if 0
        fprintf(pFile, "        { "); /* saa7108OverTable.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.DotCrawlNTSC[] */
                        pViaModeTable->saa7108OverTable[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].DotCrawlNTSC[j]);
        }
#endif
        fprintf(pFile, "        { "); /* saa7108OverTable.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_SAA7108_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_SAA7108_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.TVPAL[] */
                        pViaModeTable->saa7108OverTable[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCPAL1[] */
                        pViaModeTable->saa7108OverTable[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.MiscPAL1[] */
                        pViaModeTable->saa7108OverTable[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.MiscPAL2[] */
                        pViaModeTable->saa7108OverTable[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv3Over.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCPAL2_8BPP[] */
                        pViaModeTable->saa7108OverTable[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCPAL2_16BPP[] */
                        pViaModeTable->saa7108OverTable[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.CRTCPAL2_32BPP[] */
                        pViaModeTable->saa7108OverTable[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.PatchPAL2[] */
                        pViaModeTable->saa7108OverTable[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].PatchPAL2[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.RGBPAL[] */
                        pViaModeTable->saa7108OverTable[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].RGBPAL[j]);
        }

        fprintf(pFile, "        { "); /* saa7108OverTable.YCbCrPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* saa7108OverTable.YCbCrPAL[] */
                        pViaModeTable->saa7108OverTable[i].YCbCrPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->saa7108OverTable[i].YCbCrPAL[j]);
        }

        if (i == (VIA_BIOS_NUM_SAA7108 - 1))
            fprintf(pFile, "    }\n};\n");
        else
            fprintf(pFile, "    }, \n");
    }

    fprintf(pFile, "\n");
    fprintf(pFile, "#endif\n");

    return TRUE;
}
#endif /* CREATE_SAA7108_HEADERFILE */

#ifdef CREATE_CH7019_HEADERFILE
Bool VIACreateCH7019(VIAModeTablePtr pViaModeTable)
{
    int             i, j;
    FILE            *pFile;

    if ((pFile = fopen("via_ch7019.h", "w+")) == NULL) {
        ErrorF("Can't open \"via_ch7019.h\" file!!\n");
        return FALSE;
    }

    fprintf(pFile, "#ifndef _VIA_CH7019MODETABLE_H\n");
    fprintf(pFile, "#define _VIA_CH7019MODETABLE_H\n");
    fprintf(pFile, "\n");

    /* ch7019MaskTable */
    fprintf(pFile, "static const VIABIOSTVMASKTableRec ch7019MaskTable = {\n");
    fprintf(pFile, "    { "); /* ch7019MaskTable.TV[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_REG - 1))
            fprintf(pFile, "%#X },\n", /* ch7019MaskTable.TV[] */
                    pViaModeTable->ch7019MaskTable.TV[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->ch7019MaskTable.TV[i]);
    }

    fprintf(pFile, "    { "); /* ch7019MaskTable.CRTC1[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* ch7019MaskTable.CRTC1[] */
                    pViaModeTable->ch7019MaskTable.CRTC1[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->ch7019MaskTable.CRTC1[i]);
    }

    fprintf(pFile, "    { "); /* ch7019MaskTable.CRTC2[] */
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_CRTC; i++) {
        if (i == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
            fprintf(pFile, "%#X },\n", /* ch7019MaskTable.CRTC2[] */
                    pViaModeTable->ch7019MaskTable.CRTC2[i]);
        else
            fprintf(pFile, "%#X, ",
                    pViaModeTable->ch7019MaskTable.CRTC2[i]);
    }

    fprintf(pFile, "    %#X, %#X, %d, %d, %d \n};\n\n",
            pViaModeTable->ch7019MaskTable.misc1,
            pViaModeTable->ch7019MaskTable.misc2,
            pViaModeTable->ch7019MaskTable.numTV,
            pViaModeTable->ch7019MaskTable.numCRTC1,
            pViaModeTable->ch7019MaskTable.numCRTC2);

    /* ch7019Table */
    fprintf(pFile, "static const VIABIOSCH7019TableRec ch7019Table[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_CH7019; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* ch7019Table.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.TVNTSC[] */
                        pViaModeTable->ch7019Table[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCNTSC1[] */
                        pViaModeTable->ch7019Table[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.MiscNTSC1[] */
                        pViaModeTable->ch7019Table[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.MiscNTSC2[] */
                        pViaModeTable->ch7019Table[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCNTSC2_8BPP[] */
                        pViaModeTable->ch7019Table[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCNTSC2_16BPP[] */
                        pViaModeTable->ch7019Table[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCNTSC2_32BPP[] */
                        pViaModeTable->ch7019Table[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.PatchNTSC2[] */
                        pViaModeTable->ch7019Table[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].PatchNTSC2[j]);
        }
#if 0
        fprintf(pFile, "        { "); /* ch7019Table.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.RGBNTSC[] */
                        pViaModeTable->ch7019Table[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].RGBNTSC[j]);
        }
#endif
        fprintf(pFile, "        { "); /* ch7019Table.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.DotCrawlNTSC[] */
                        pViaModeTable->ch7019Table[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.TVPAL[] */
                        pViaModeTable->ch7019Table[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCPAL1[] */
                        pViaModeTable->ch7019Table[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.MiscPAL1[] */
                        pViaModeTable->ch7019Table[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.MiscPAL2[] */
                        pViaModeTable->ch7019Table[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCPAL2_8BPP[] */
                        pViaModeTable->ch7019Table[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCPAL2_16BPP[] */
                        pViaModeTable->ch7019Table[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.CRTCPAL2_32BPP[] */
                        pViaModeTable->ch7019Table[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019Table.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.PatchPAL2[] */
                        pViaModeTable->ch7019Table[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].PatchPAL2[j]);
        }
#if 0
        fprintf(pFile, "        { "); /* ch7019Table.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* ch7019Table.RGBPAL[] */
                        pViaModeTable->ch7019Table[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019Table[i].RGBPAL[j]);
        }
#endif
        if (i == (VIA_BIOS_NUM_CH7019 - 1))
            fprintf(pFile, "    }\n};\n\n");
        else
            fprintf(pFile, "    }, \n");
    }

    /* ch7019OverTable */
    fprintf(pFile, "static const VIABIOSCH7019TableRec ch7019OverTable[] = {\n");
    for (i = 0; i < VIA_BIOS_NUM_CH7019; i++) {
        fprintf(pFile, "    {\n");
        fprintf(pFile, "        { "); /* ch7019OverTable.TVNTSC[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.TVNTSC[] */
                        pViaModeTable->ch7019OverTable[i].TVNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].TVNTSC[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.CRTCNTSC1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCNTSC1[] */
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.MiscNTSC1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.MiscNTSC1[] */
                        pViaModeTable->ch7019OverTable[i].MiscNTSC1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].MiscNTSC1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.MiscNTSC2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.MiscNTSC2[] */
                        pViaModeTable->ch7019OverTable[i].MiscNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].MiscNTSC2[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.CRTCNTSC2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCNTSC2_8BPP[] */
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.CRTCNTSC2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCNTSC2_16BPP[] */
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.CRTCNTSC2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCNTSC2_32BPP[] */
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCNTSC2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.PatchNTSC2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.PatchNTSC2[] */
                        pViaModeTable->ch7019OverTable[i].PatchNTSC2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].PatchNTSC2[j]);
        }
#if 0
        fprintf(pFile, "        { "); /* ch7019OverTable.RGBNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.RGBNTSC[] */
                        pViaModeTable->ch7019OverTable[i].RGBNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].RGBNTSC[j]);
        }
#endif
        fprintf(pFile, "        { "); /* ch7019OverTable.DotCrawlNTSC[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.DotCrawlNTSC[] */
                        pViaModeTable->ch7019OverTable[i].DotCrawlNTSC[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].DotCrawlNTSC[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.TVPAL[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.TVPAL[] */
                        pViaModeTable->ch7019OverTable[i].TVPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].TVPAL[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.CRTCPAL1[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCPAL1[] */
                        pViaModeTable->ch7019OverTable[i].CRTCPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCPAL1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.MiscPAL1[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.MiscPAL1[] */
                        pViaModeTable->ch7019OverTable[i].MiscPAL1[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].MiscPAL1[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.MiscPAL2[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_SPECIAL_REG; j++) {
            if (j == (VIA_BIOS_NUM_TV_SPECIAL_REG - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.MiscPAL2[] */
                        pViaModeTable->ch7019OverTable[i].MiscPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].MiscPAL2[j]);
        }

        fprintf(pFile, "        { "); /* tv3Over.CRTCPAL2_8BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCPAL2_8BPP[] */
                        pViaModeTable->ch7019OverTable[i].CRTCPAL2_8BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCPAL2_8BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.CRTCPAL2_16BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCPAL2_16BPP[] */
                        pViaModeTable->ch7019OverTable[i].CRTCPAL2_16BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCPAL2_16BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.CRTCPAL2_32BPP[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_CRTC; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_CRTC - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.CRTCPAL2_32BPP[] */
                        pViaModeTable->ch7019OverTable[i].CRTCPAL2_32BPP[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].CRTCPAL2_32BPP[j]);
        }

        fprintf(pFile, "        { "); /* ch7019OverTable.PatchPAL2[] */
        for (j = 0; j < VIA_BIOS_MAX_NUM_TV_PATCH; j++) {
            if (j == (VIA_BIOS_MAX_NUM_TV_PATCH - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.PatchPAL2[] */
                        pViaModeTable->ch7019OverTable[i].PatchPAL2[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].PatchPAL2[j]);
        }
#if 0
        fprintf(pFile, "        { "); /* ch7019OverTable.RGBPAL[] */
        for (j = 0; j < VIA_BIOS_NUM_TV_OTHER; j++) {
            if (j == (VIA_BIOS_NUM_TV_OTHER - 1))
                fprintf(pFile, "%#X },\n", /* ch7019OverTable.RGBPAL[] */
                        pViaModeTable->ch7019OverTable[i].RGBPAL[j]);
            else
                fprintf(pFile, "%#X, ",
                        pViaModeTable->ch7019OverTable[i].RGBPAL[j]);
        }
#endif
        if (i == (VIA_BIOS_NUM_CH7019 - 1))
            fprintf(pFile, "    }\n};\n");
        else
            fprintf(pFile, "    }, \n");
    }

    fprintf(pFile, "\n");
    fprintf(pFile, "#endif\n");

    return TRUE;
}
#endif /* CREATE_CH7019_HEADERFILE */

void VIAGetCH7019Mask(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    unsigned char       *pRom;
    int                 i, j, k, m;
    CARD16              tmp;

    DEBUG(xf86Msg(X_INFO, "VIAGetCH7019Mask\n"));
    /* Get start of TV Mask Table */
    pRom = pTable + VIA_BIOS_TVMASKTAB_POS;
    DEBUG(xf86Msg(X_INFO, "csTVMaskTbl: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    for (i = 0, j = 0, k = 0; i < 9; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->ch7019MaskTable.TV[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->ch7019MaskTable.TV[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->ch7019MaskTable.numTV = k;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->ch7019MaskTable.CRTC1[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->ch7019MaskTable.CRTC1[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->ch7019MaskTable.numCRTC1 = k;

    pViaModeTable->ch7019MaskTable.misc1 = *pRom++;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->ch7019MaskTable.CRTC2[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->ch7019MaskTable.CRTC2[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->ch7019MaskTable.numCRTC2 = k;

    pViaModeTable->ch7019MaskTable.misc2 = *pRom++;

}

void VIAGetCH7019NTSC(VIAModeTablePtr  pViaModeTable,
                     unsigned char  *pBIOS,
                     unsigned char  *pTable)
{
    VIABIOSCH7019TablePtr  pCH7019Tab;
    unsigned char       *pRom, *pNTSC;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    unsigned char       numPatch, numReg;
    int                 vScan, offset;
    int                 i, j, k, n;

    pCH7019Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetCH7019NTSC\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pCH7019Tab = pViaModeTable->ch7019Table;
                /* HSoffset = 2; */
                break;
            case VIA_TVOVER:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeOverTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pCH7019Tab = pViaModeTable->ch7019OverTable;
                /* HSoffset = 38; */
                break;
        }

        /* offset: skip MODE3, MODE13 */
        for (offset = 11, n = 0; n < VIA_BIOS_NUM_CH7019; offset += 5, n++) {

            /* Get pointer table of each mode */
            pRom = pTVTable + offset;

            if ((*((CARD16 *)pRom)) != 0) {
                pNTSC = pBIOS + *((CARD16 *)pRom);

                /* Get start of TV Table */
                pRom = pNTSC + 1;
                pRom = pBIOS + *((CARD16 *)pRom);

                for (i = 0, j = 0; i < pViaModeTable->ch7019MaskTable.numTV; j++) {
                    if (pViaModeTable->ch7019MaskTable.TV[j] == 0xFF) {
                        pCH7019Tab[n].TVNTSC[j] = *pRom++;
                        i++;
                    }
                }

                numCRTC = *pRom++;

                for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                    if (pViaModeTable->ch7019MaskTable.CRTC1[j] == 0xFF) {
                        if (i >= pViaModeTable->ch7019MaskTable.numCRTC1) {
                            pCH7019Tab[n].MiscNTSC1[k++] = *pRom++;
                            i++;
                        }
                        else {
                            pCH7019Tab[n].CRTCNTSC1[j] = *pRom++;
                            i++;
                        }
                    }
                }

                for (i = 0, j = 0, k = 0; i < pViaModeTable->ch7019MaskTable.numCRTC2; j++) {
                    if (pViaModeTable->ch7019MaskTable.CRTC2[j] == 0xFF) {
                        /* CRTC 65-57 8bpp, 16bpp, 32bpp */
                        if (j == 0x15) {
                            pCH7019Tab[n].CRTCNTSC2_8BPP[j] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_8BPP[j+1] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_8BPP[j+2] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_16BPP[j] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_16BPP[j+1] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_16BPP[j+2] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                            pCH7019Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                            i += 3;
                        }
                        else {
                            /* For CRTC 6A-6C */
                            if (i >= (pViaModeTable->ch7019MaskTable.numCRTC2 - 3)) {
                                pCH7019Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                            else {
                                pCH7019Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                        }
                    }
                }

                k = 3;
                /* LCK 3c5.44-45 */
                if (pViaModeTable->ch7019MaskTable.misc2 & 0x18) {
                        pCH7019Tab[n].MiscNTSC2[k++] = *pRom++;
                        pCH7019Tab[n].MiscNTSC2[k++] = *pRom++;
                }

                /* Patch as setting 2nd path */
                numPatch = (int)(pViaModeTable->ch7019MaskTable.misc2 >> 5);
                for (i = 0; i < numPatch; i++) {
                    pCH7019Tab[n].PatchNTSC2[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

                /* Get DotCrawl Table */
                pRom = pNTSC + 4;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pCH7019Tab[n].DotCrawlNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pCH7019Tab[n].DotCrawlNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }
            }
        }
    }
}

void VIAGetCH7019PAL(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    VIABIOSCH7019TablePtr  pCH7019Tab;
    unsigned char       *pRom, *pPAL;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    unsigned char       numPatch;
    int                 vScan, offset;
    int                 i, j, k, n;

    pCH7019Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetSAA7108PAL\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pCH7019Tab = pViaModeTable->ch7019Table;
                /* HSoffset = 4; */
                break;
            case VIA_TVOVER:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pCH7019Tab = pViaModeTable->ch7019OverTable;
                /* HSoffset = 40; */
                break;
        }

        /* offset: skip MODE3, MODE13 */
        for (offset = 13, n = 0; n < VIA_BIOS_NUM_CH7019; offset += 5, n++) {

            /* Get pointer table of each mode */
            pRom = pTVTable + offset;

            if (*((CARD16 *)pRom) != 0) {
                pPAL = pBIOS + *((CARD16 *)pRom);

                /* Get start of TV Table */
                pRom = pPAL + 1;
                pRom = pBIOS + *((CARD16 *)pRom);

                for (i = 0, j = 0; i < pViaModeTable->ch7019MaskTable.numTV; j++) {
                    if (pViaModeTable->ch7019MaskTable.TV[j] == 0xFF) {
                        pCH7019Tab[n].TVPAL[j] = *pRom++;
                        i++;
                    }
                }

                numCRTC = *pRom++;

                for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                    if (pViaModeTable->ch7019MaskTable.CRTC1[j] == 0xFF) {
                        if (i >= pViaModeTable->ch7019MaskTable.numCRTC1) {
                            pCH7019Tab[n].MiscPAL1[k++] = *pRom++;
                            i++;
                        }
                        else {
                            pCH7019Tab[n].CRTCPAL1[j] = *pRom++;
                            i++;
                        }
                    }
                }

                for (i = 0, j = 0, k = 0; i < pViaModeTable->ch7019MaskTable.numCRTC2; j++) {
                    if (pViaModeTable->ch7019MaskTable.CRTC2[j] == 0xFF) {
                        if (j == 0x15) {
                            pCH7019Tab[n].CRTCPAL2_8BPP[j] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_8BPP[j+1] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_8BPP[j+2] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_16BPP[j] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_16BPP[j+1] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_16BPP[j+2] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                            pCH7019Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                            i += 3;
                        }
                        else {
                            /* For CRTC 6A-6C */
                            if (i >= (pViaModeTable->ch7019MaskTable.numCRTC2 - 3)) {
                                pCH7019Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                                i++;
                            }
                            else {
                                pCH7019Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                                pCH7019Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                                i++;
                            }
                        }
                    }
                }

                k = 3;
                /* LCK 3c5.44-45 */
                if (pViaModeTable->ch7019MaskTable.misc2 & 0x18) {
                    pCH7019Tab[n].MiscPAL2[k++] = *pRom++;
                    pCH7019Tab[n].MiscPAL2[k++] = *pRom++;
                }

                /* Patch as setting 2nd path */
                numPatch = (int)(pViaModeTable->ch7019MaskTable.misc2 >> 5);
                for (i = 0; i < numPatch; i++) {
                    pCH7019Tab[n].PatchPAL2[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }
            }
        }
    }
}

void VIAGetSAA7108Mask(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    unsigned char       *pRom;
    int                 i, j, k, m;
    CARD16              tmp;

    DEBUG(xf86Msg(X_INFO, "VIAGetSAA7108Mask\n"));
    /* Get start of TV Mask Table */
    pRom = pTable + VIA_BIOS_TVMASKTAB_POS;
    DEBUG(xf86Msg(X_INFO, "csTVMaskTbl: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    for (i = 0, j = 0, k = 0; i < 11; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->saa7108MaskTable.TV[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->saa7108MaskTable.TV[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->saa7108MaskTable.numTV = k;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->saa7108MaskTable.CRTC1[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->saa7108MaskTable.CRTC1[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->saa7108MaskTable.numCRTC1 = k;

    pViaModeTable->saa7108MaskTable.misc1 = *pRom++;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->saa7108MaskTable.CRTC2[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->saa7108MaskTable.CRTC2[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->saa7108MaskTable.numCRTC2 = k;

    pViaModeTable->saa7108MaskTable.misc2 = *pRom++;

}

void VIAGetSAA7108NTSC(VIAModeTablePtr  pViaModeTable,
                     unsigned char  *pBIOS,
                     unsigned char  *pTable)
{
    VIABIOSSAA7108TablePtr  pSAA7108Tab;
    unsigned char       *pRom, *pNTSC;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    unsigned char       numPatch, numReg;
    int                 vScan, offset;
    int                 i, j, k, n;

    pSAA7108Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetSAA7108NTSC\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pSAA7108Tab = pViaModeTable->saa7108Table;
                /* HSoffset = 2; */
                break;
            case VIA_TVOVER:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeOverTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pSAA7108Tab = pViaModeTable->saa7108OverTable;
                /* HSoffset = 38; */
                break;
        }

        /* offset: skip MODE3, MODE13 */
        for (offset = 11, n = 0; n < VIA_BIOS_NUM_SAA7108; offset += 5, n++) {

            /* Get pointer table of each mode */
            pRom = pTVTable + offset;

            if ((*((CARD16 *)pRom)) != 0) {
                pNTSC = pBIOS + *((CARD16 *)pRom);

                /* Get start of TV Table */
                pRom = pNTSC + 1;
                pRom = pBIOS + *((CARD16 *)pRom);

                for (i = 0, j = 0; i < pViaModeTable->saa7108MaskTable.numTV; j++) {
                    if (pViaModeTable->saa7108MaskTable.TV[j] == 0xFF) {
                        pSAA7108Tab[n].TVNTSC[j] = *pRom++;
                        i++;
                    }
                }

                numCRTC = *pRom++;

                for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                    if (pViaModeTable->saa7108MaskTable.CRTC1[j] == 0xFF) {
                        if (i >= pViaModeTable->saa7108MaskTable.numCRTC1) {
                            pSAA7108Tab[n].MiscNTSC1[k++] = *pRom++;
                            i++;
                        }
                        else {
                            pSAA7108Tab[n].CRTCNTSC1[j] = *pRom++;
                            i++;
                        }
                    }
                }

                for (i = 0, j = 0, k = 0; i < pViaModeTable->saa7108MaskTable.numCRTC2; j++) {
                    if (pViaModeTable->saa7108MaskTable.CRTC2[j] == 0xFF) {
                        /* CRTC 65-57 8bpp, 16bpp, 32bpp */
                        if (j == 0x15) {
                            pSAA7108Tab[n].CRTCNTSC2_8BPP[j] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_8BPP[j+1] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_8BPP[j+2] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_16BPP[j] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_16BPP[j+1] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_16BPP[j+2] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                            pSAA7108Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                            i += 3;
                        }
                        else {
                            /* For CRTC 6A-6C */
                            if (i >= (pViaModeTable->saa7108MaskTable.numCRTC2 - 3)) {
                                pSAA7108Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                            else {
                                pSAA7108Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                        }
                    }
                }

                k = 3;
                /* LCK 3c5.44-45 */
                if (pViaModeTable->saa7108MaskTable.misc2 & 0x18) {
                        pSAA7108Tab[n].MiscNTSC2[k++] = *pRom++;
                        pSAA7108Tab[n].MiscNTSC2[k++] = *pRom++;
                }

                /* Patch as setting 2nd path */
                numPatch = (int)(pViaModeTable->saa7108MaskTable.misc2 >> 5);
                for (i = 0; i < numPatch; i++) {
                    pSAA7108Tab[n].PatchNTSC2[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

                /* Get RGB Table */
                pRom = pNTSC + 4;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pSAA7108Tab[n].RGBNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pSAA7108Tab[n].RGBNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

                /* Get YCbCr Table */
                pRom = pNTSC + 7;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pSAA7108Tab[n].YCbCrNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pSAA7108Tab[n].YCbCrNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }
            }
        }
    }
}

void VIAGetSAA7108PAL(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    VIABIOSSAA7108TablePtr  pSAA7108Tab;
    unsigned char       *pRom, *pPAL;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    unsigned char       numPatch, numReg;
    int                 vScan, offset;
    int                 i, j, k, n;

    pSAA7108Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetSAA7108PAL\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pSAA7108Tab = pViaModeTable->saa7108Table;
                /* HSoffset = 4; */
                break;
            case VIA_TVOVER:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pSAA7108Tab = pViaModeTable->saa7108OverTable;
                /* HSoffset = 40; */
                break;
        }

        /* offset: skip MODE3, MODE13 */
        for (offset = 13, n = 0; n < VIA_BIOS_NUM_SAA7108; offset += 5, n++) {

            /* Get pointer table of each mode */
            pRom = pTVTable + offset;

            if (*((CARD16 *)pRom) != 0) {
                pPAL = pBIOS + *((CARD16 *)pRom);

                /* Get start of TV Table */
                pRom = pPAL + 1;
                pRom = pBIOS + *((CARD16 *)pRom);

                for (i = 0, j = 0; i < pViaModeTable->saa7108MaskTable.numTV; j++) {
                    if (pViaModeTable->saa7108MaskTable.TV[j] == 0xFF) {
                        pSAA7108Tab[n].TVPAL[j] = *pRom++;
                        i++;
                    }
                }

                numCRTC = *pRom++;

                for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                    if (pViaModeTable->saa7108MaskTable.CRTC1[j] == 0xFF) {
                        if (i >= pViaModeTable->saa7108MaskTable.numCRTC1) {
                            pSAA7108Tab[n].MiscPAL1[k++] = *pRom++;
                            i++;
                        }
                        else {
                            pSAA7108Tab[n].CRTCPAL1[j] = *pRom++;
                            i++;
                        }
                    }
                }

                for (i = 0, j = 0, k = 0; i < pViaModeTable->saa7108MaskTable.numCRTC2; j++) {
                    if (pViaModeTable->saa7108MaskTable.CRTC2[j] == 0xFF) {
                        if (j == 0x15) {
                            pSAA7108Tab[n].CRTCPAL2_8BPP[j] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_8BPP[j+1] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_8BPP[j+2] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_16BPP[j] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_16BPP[j+1] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_16BPP[j+2] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                            pSAA7108Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                            i += 3;
                        }
                        else {
                            /* For CRTC 6A-6C */
                            if (i >= (pViaModeTable->saa7108MaskTable.numCRTC2 - 3)) {
                                pSAA7108Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                                i++;
                            }
                            else {
                                pSAA7108Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                                pSAA7108Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                                i++;
                            }
                        }
                    }
                }

                k = 3;
                /* LCK 3c5.44-45 */
                if (pViaModeTable->saa7108MaskTable.misc2 & 0x18) {
                    pSAA7108Tab[n].MiscPAL2[k++] = *pRom++;
                    pSAA7108Tab[n].MiscPAL2[k++] = *pRom++;
                }

                /* Patch as setting 2nd path */
                numPatch = (int)(pViaModeTable->saa7108MaskTable.misc2 >> 5);
                for (i = 0; i < numPatch; i++) {
                    pSAA7108Tab[n].PatchPAL2[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

                /* Get RGB Table */
                pRom = pPAL + 4;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pSAA7108Tab[n].RGBPAL[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pSAA7108Tab[n].RGBPAL[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

                /* Get YCbCr Table */
                pRom = pPAL + 7;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pSAA7108Tab[n].YCbCrPAL[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pSAA7108Tab[n].YCbCrPAL[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }
            }
        }
    }
}

void VIAGetTV2Mask(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    unsigned char       *pRom;
    int                 i, j, k, m;
    CARD16              tmp;

    DEBUG(xf86Msg(X_INFO, "VIAGetTV2Mask\n"));
    /* Get start of TV Mask Table */
    pRom = pTable + VIA_BIOS_TVMASKTAB_POS;
    pRom = pBIOS + *((CARD16 *)pRom);

    for (i = 0, j = 0, k = 0; i < 9; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->tv2MaskTable.TV[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->tv2MaskTable.TV[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->tv2MaskTable.numTV = k;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->tv2MaskTable.CRTC1[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->tv2MaskTable.CRTC1[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->tv2MaskTable.numCRTC1 = k;

    pViaModeTable->tv2MaskTable.misc1 = *pRom++;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->tv2MaskTable.CRTC2[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->tv2MaskTable.CRTC2[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->tv2MaskTable.numCRTC2 = k;

    pViaModeTable->tv2MaskTable.misc2 = *pRom++;
}


void VIAGetTV3Mask(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    unsigned char       *pRom;
    int                 i, j, k, m;
    CARD16              tmp;

    DEBUG(xf86Msg(X_INFO, "VIAGetTV3Mask\n"));
    /* Get start of TV Mask Table */
    pRom = pTable + VIA_BIOS_TVMASKTAB_POS;
    DEBUG(xf86Msg(X_INFO, "csTVMaskTbl: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    for (i = 0, j = 0, k = 0; i < 9; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->tv3MaskTable.TV[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->tv3MaskTable.TV[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->tv3MaskTable.numTV = k;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->tv3MaskTable.CRTC1[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->tv3MaskTable.CRTC1[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->tv3MaskTable.numCRTC1 = k;

    pViaModeTable->tv3MaskTable.misc1 = *pRom++;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->tv3MaskTable.CRTC2[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->tv3MaskTable.CRTC2[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->tv3MaskTable.numCRTC2 = k;

    pViaModeTable->tv3MaskTable.misc2 = *pRom++;

}


void VIAGetTV2NTSC(VIAModeTablePtr pViaModeTable,
                    unsigned char *pBIOS,
                    unsigned char *pTable)
{
    VIABIOSTV2TablePtr  pTV2Tab;
    unsigned char       *pRom, *pNTSC;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    unsigned char       numPatch, numReg;
    int                 vScan, offset;
    int                 i, j, k, n;

    pTV2Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetTV2NTSC\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get start of TV Table */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV2Tab = pViaModeTable->tv2Table;
                break;
            case VIA_TVOVER:
                /* Get start of TV Table */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV2Tab = pViaModeTable->tv2OverTable;
                break;
        }

        for (offset = 16, n = 0; n < VIA_BIOS_NUM_TV2; offset += 5, n++) {

            pRom = pTVTable + offset;
            pNTSC = pBIOS + *((CARD16 *)pRom);

            pRom = pNTSC + 1;
            pRom = pBIOS + *((CARD16 *)pRom);

            for (i = 0, j = 0; i < pViaModeTable->tv2MaskTable.numTV; j++) {
                if (pViaModeTable->tv2MaskTable.TV[j] == 0xFF) {
                    pTV2Tab[n].TVNTSCC[j] = *pRom++;
                    i++;
                }
            }

            for (i = 0, j = 0; i < pViaModeTable->tv2MaskTable.numTV; j++) {
                if (pViaModeTable->tv2MaskTable.TV[j] == 0xFF) {
                    if (j >= 0x53) {
                        pTV2Tab[n].TVNTSCS[j] = *pRom++;
                        i++;
                    }
                    else {
                        pTV2Tab[n].TVNTSCS[j] =
                        pTV2Tab[n].TVNTSCC[j];
                        i++;
                    }
                }
            }

            numCRTC = *pRom++;

            for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                if (pViaModeTable->tv2MaskTable.CRTC1[j] == 0xFF) {
                    if (i >= pViaModeTable->tv2MaskTable.numCRTC1) {
                        pTV2Tab[n].MiscNTSC1[k++] = *pRom++;
                        i++;
                    }
                    else {
                        pTV2Tab[n].CRTCNTSC1[j] = *pRom++;
                        i++;
                    }
                }
            }

            for (i = 0, j = 0, k = 0; i < pViaModeTable->tv2MaskTable.numCRTC2; j++) {
                if (pViaModeTable->tv2MaskTable.CRTC2[j] == 0xFF) {
                    /* CRTC 65-57 8bpp, 16bpp, 32bpp */
                    if (j == 0x15) {
                        pTV2Tab[n].CRTCNTSC2_8BPP[j] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_8BPP[j+1] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_8BPP[j+2] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_16BPP[j] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_16BPP[j+1] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_16BPP[j+2] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                        pTV2Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                        i += 3;
                    }
                    else {
                        /* For CRTC 6A-6C */
                        if (i >= (pViaModeTable->tv2MaskTable.numCRTC2 - 3)) {
                            pTV2Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                            pTV2Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                            pTV2Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                            i++;
                        }
                        else {
                            pTV2Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                            pTV2Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                            pTV2Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                            i++;
                        }
                    }
                }
            }

            k = 3;
            /* LCK 3c5.44-45 */
            if (pViaModeTable->tv2MaskTable.misc2 & 0x18) {
                pTV2Tab[n].MiscNTSC2[k++] = *pRom++;
                pTV2Tab[n].MiscNTSC2[k++] = *pRom++;
            }

            /* Patch as setting 2nd path */
            numPatch = (int)(pViaModeTable->tv2MaskTable.misc2 >> 5);
            for (i = 0; i < numPatch; i++) {
                pTV2Tab[n].PatchNTSC2[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get DotCrawl Table */
            pRom = pNTSC + 4;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
            pTV2Tab[n].DotCrawlNTSC[0] = numReg;
            for ( i = 1; i < (numReg + 1); i++) {
                pTV2Tab[n].DotCrawlNTSC[i] = *((CARD16 *)pRom);
                pRom += 2;
            }
        }
    }
}

void VIAGetTV2PAL(VIAModeTablePtr pViaModeTable,
                    unsigned char *pBIOS,
                    unsigned char *pTable)
{
    VIABIOSTV2TablePtr  pTV2Tab;
    unsigned char       *pRom, *pPAL;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    int                 vScan, offset;
    int                 numPatch;
    int                 i, j, k, n;

    pTV2Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetTV2PAL\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get start of TV Table */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV2Tab = pViaModeTable->tv2Table;
                break;
            case VIA_TVOVER:
                /* Get start of TV Table */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV2Tab = pViaModeTable->tv2OverTable;
                break;
        }

        for (offset = 18, n = 0; n < VIA_BIOS_NUM_TV2; offset += 5, n++) {

            pRom = pTVTable + offset;
            pPAL = pBIOS + *((CARD16 *)pRom);

            pRom = pPAL + 1;
            pRom = pBIOS + *((CARD16 *)pRom);

            for (i = 0, j = 0; i < pViaModeTable->tv2MaskTable.numTV; j++) {
                if (pViaModeTable->tv2MaskTable.TV[j] == 0xFF) {
                    pTV2Tab[n].TVPALC[j] = *pRom++;
                    i++;
                }
            }

            for (i = 0, j = 0; i < pViaModeTable->tv2MaskTable.numTV; j++) {
                if (pViaModeTable->tv2MaskTable.TV[j] == 0xFF) {
                    if (j >= 0x53) {
                        pTV2Tab[n].TVPALS[j] = *pRom++;
                        i++;
                    }
                    else {
                        pTV2Tab[n].TVPALS[j] =
                        pTV2Tab[n].TVPALC[j];
                        i++;
                    }
                }
            }

            numCRTC = *pRom++;

            for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                if (pViaModeTable->tv2MaskTable.CRTC1[j] == 0xFF) {
                    if (i >= pViaModeTable->tv2MaskTable.numCRTC1) {
                        pTV2Tab[n].MiscPAL1[k++] = *pRom++;
                        i++;
                    }
                    else {
                        pTV2Tab[n].CRTCPAL1[j] = *pRom++;
                        i++;
                    }
                }
            }

            for (i = 0, j = 0, k = 0; i < pViaModeTable->tv2MaskTable.numCRTC2; j++) {
                if (pViaModeTable->tv2MaskTable.CRTC2[j] == 0xFF) {
                    /* CRTC 65-57 8bpp, 16bpp, 32bpp */
                    if (j == 0x15) {
                        pTV2Tab[n].CRTCPAL2_8BPP[j] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_8BPP[j+1] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_8BPP[j+2] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_16BPP[j] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_16BPP[j+1] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_16BPP[j+2] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                        pTV2Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                        i += 3;
                    }
                    else {
                        /* For CRTC 6A-6C */
                        if (i >= (pViaModeTable->tv2MaskTable.numCRTC2 - 3)) {
                            pTV2Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                            pTV2Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                            pTV2Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                            i++;
                        }
                        else {
                            pTV2Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                            pTV2Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                            pTV2Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                            i++;
                        }
                    }
                }
            }

            k = 3;
            /* LCK 3c5.44-45 */
            if (pViaModeTable->tv2MaskTable.misc2 & 0x18) {
                pTV2Tab[n].MiscPAL2[k++] = *pRom++;
                pTV2Tab[n].MiscPAL2[k++] = *pRom++;
           }

            /* Patch as setting 2nd path */
            numPatch = (int)(pViaModeTable->tv2MaskTable.misc2 >> 5);
            for (i = 0; i < numPatch; i++) {
                pTV2Tab[n].PatchPAL2[i] = *((CARD16 *)pRom);
                pRom += 2;
            }
        }
    }
}

void VIAGetTV3NTSC(VIAModeTablePtr  pViaModeTable,
                     unsigned char  *pBIOS,
                     unsigned char  *pTable)
{
    VIABIOSTV3TablePtr  pTV3Tab;
    unsigned char       *pRom, *pNTSC;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    unsigned char       numPatch, numReg;
    int                 vScan, offset;
    int                 i, j, k, n;

    pTV3Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetTV3NTSC\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV3Tab = pViaModeTable->tv3Table;
                /* HSoffset = 2; */
                break;
            case VIA_TVOVER:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeOverTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV3Tab = pViaModeTable->tv3OverTable;
                /* HSoffset = 38; */
                break;
        }

        /* offset: skip MODE3, MODE13 */
        for (offset = 11, n = 0; n < VIA_BIOS_NUM_TV3; offset += 5, n++) {

            /* Get pointer table of each mode */
            pRom = pTVTable + offset;

            if ((*((CARD16 *)pRom)) != 0) {

            pNTSC = pBIOS + *((CARD16 *)pRom);

            /* Get start of TV Table */
            pRom = pNTSC + 1;
            pRom = pBIOS + *((CARD16 *)pRom);

            for (i = 0, j = 0; i < pViaModeTable->tv3MaskTable.numTV; j++) {
                if (pViaModeTable->tv3MaskTable.TV[j] == 0xFF) {
                        pTV3Tab[n].TVNTSC[j] = *pRom++;
                    i++;
                }
            }

            numCRTC = *pRom++;

            for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                if (pViaModeTable->tv3MaskTable.CRTC1[j] == 0xFF) {
                    if (i >= pViaModeTable->tv3MaskTable.numCRTC1) {
                            pTV3Tab[n].MiscNTSC1[k++] = *pRom++;
                            i++;
                        }
                        else {
                            pTV3Tab[n].CRTCNTSC1[j] = *pRom++;
                            i++;
                        }
                    }
                }

            for (i = 0, j = 0, k = 0; i < pViaModeTable->tv3MaskTable.numCRTC2; j++) {
                if (pViaModeTable->tv3MaskTable.CRTC2[j] == 0xFF) {
                    /* CRTC 65-57 8bpp, 16bpp, 32bpp */
                    if (j == 0x15) {
                            pTV3Tab[n].CRTCNTSC2_8BPP[j] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_8BPP[j+1] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_8BPP[j+2] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_16BPP[j] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_16BPP[j+1] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_16BPP[j+2] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                            pTV3Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                        i += 3;
                    }
                    else {
                        /* For CRTC 6A-6C */
                        if (i >= (pViaModeTable->tv3MaskTable.numCRTC2 - 3)) {
                                pTV3Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pTV3Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pTV3Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                            else {
                                pTV3Tab[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pTV3Tab[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pTV3Tab[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                        }
                    }
                }

            k = 3;
            /* LCK 3c5.44-45 */
            if (pViaModeTable->tv3MaskTable.misc2 & 0x18) {
                    pTV3Tab[n].MiscNTSC2[k++] = *pRom++;
                    pTV3Tab[n].MiscNTSC2[k++] = *pRom++;
            }

            /* Patch as setting 2nd path */
            numPatch = (int)(pViaModeTable->tv3MaskTable.misc2 >> 5);
            for (i = 0; i < numPatch; i++) {
                    pTV3Tab[n].PatchNTSC2[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get RGB Table */
            pRom = pNTSC + 4;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].RGBNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].RGBNTSC[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get YCbCr Table */
            pRom = pNTSC + 7;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].YCbCrNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].YCbCrNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

#if 0
            /* Get SDTV_RGB Table */
            pRom = pNTSC + 10;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].SDTV_RGBNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].SDTV_RGBNTSC[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get SDTV_YCbCr Table */
            pRom = pNTSC + 13;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].SDTV_YCbCrNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].SDTV_YCbCrNTSC[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get DotCrawl Table */
            pRom = pNTSC + 16;
            pRom = pBIOS + *((CARD16 *)pRom);
#endif

                /* Get DotCrawl Table */
                pRom = pNTSC + 10;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pTV3Tab[n].DotCrawlNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].DotCrawlNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }
            }
        }
    }
}

void VIAGetFS454Mask(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    unsigned char       *pRom;
    int                 i, j, k, m;
    CARD16              tmp;

    DEBUG(xf86Msg(X_INFO, "VIAGetFS454Mask\n"));
    /* Get start of TV Mask Table */
    pRom = pTable + VIA_BIOS_TVMASKTAB_POS;
	DEBUG(xf86Msg(X_INFO, "csTVMaskTbl: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

	/* Skip Zero TV Register Mask Table */
	pRom += (9*2);

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->fs454MaskTable.CRTC1[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->fs454MaskTable.CRTC1[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->fs454MaskTable.numCRTC1 = k;

    pViaModeTable->fs454MaskTable.misc1 = *pRom++;

    for (i = 0, j = 0, k = 0; i < 2; i++) {
        tmp = *((CARD16 *)pRom);
        for (m = 0; m < 16; m++, j++) {
            if ((tmp >> m) & 0x01) {
                pViaModeTable->fs454MaskTable.CRTC2[j] = 0xFF;
                k++;
            }
            else {
                pViaModeTable->fs454MaskTable.CRTC2[j] = 0;
            }
        }
        pRom += 2;
    }
    pViaModeTable->fs454MaskTable.numCRTC2 = k;

    pViaModeTable->fs454MaskTable.misc2 = *pRom++;

}

void VIAGetFS454NTSC(VIAModeTablePtr  pViaModeTable,
                     unsigned char  *pBIOS,
                     unsigned char  *pTable)
{
    VIABIOSFS454TablePtr  pFS454Tbl;
    unsigned char       *pRom, *pNTSC;
    unsigned char       *pTVTable;
    unsigned char       numCRTC, numReg;
    int                 vScan, offset;
    int                 i, j, k, n;

    pFS454Tbl = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetFS454NTSC\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pFS454Tbl = pViaModeTable->fs454Table;
                /* HSoffset = 2; */
                break;
            case VIA_TVOVER:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                DEBUG(xf86Msg(X_INFO, "csTVModeOverTbl: %X\n", *((CARD16 *)pRom)));
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pFS454Tbl = pViaModeTable->fs454OverTable;
                /* HSoffset = 38; */
                break;
        }

        /* offset: skip MODE3, MODE13 */
        for (offset = 11, n = 0; n < VIA_BIOS_NUM_FS454; offset += 5, n++) {

            /* Get pointer table of each mode */
            pRom = pTVTable + offset;

            if ((*((CARD16 *)pRom)) != 0) {

            pNTSC = pBIOS + *((CARD16 *)pRom);

            /* Get start of TV Table */
            pRom = pNTSC + 1;
            pRom = pBIOS + *((CARD16 *)pRom);

			numReg = *pRom++;
			pFS454Tbl[n].TVNTSC[0] = numReg;
            for (i = 1; i < (numReg + 1); i++) {
                pFS454Tbl[n].TVNTSC[i] = *((CARD16 *)pRom);
            	pRom += 2;
            }

            numCRTC = *pRom++;

            for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                if (pViaModeTable->fs454MaskTable.CRTC1[j] == 0xFF) {
                    if (i >= pViaModeTable->fs454MaskTable.numCRTC1) {
                        pFS454Tbl[n].MiscNTSC1[k++] = *pRom++;
                        i++;
                    }
                    else {
                        pFS454Tbl[n].CRTCNTSC1[j] = *pRom++;
                        i++;
                    }
                }
            }

            for (i = 0, j = 0, k = 0; i < pViaModeTable->fs454MaskTable.numCRTC2; j++) {
                if (pViaModeTable->fs454MaskTable.CRTC2[j] == 0xFF) {
                    /* CRTC 65-57 8bpp, 16bpp, 32bpp */
                    if (j == 0x15) {
                        pFS454Tbl[n].CRTCNTSC2_8BPP[j] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_8BPP[j+1] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_8BPP[j+2] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_16BPP[j] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_16BPP[j+1] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_16BPP[j+2] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_32BPP[j++] = *pRom++;
                        pFS454Tbl[n].CRTCNTSC2_32BPP[j] = *pRom++;
                        i += 3;
                    }
                    else {
                        /* For CRTC 6A-6C */
                        if (i >= (pViaModeTable->fs454MaskTable.numCRTC2 - 3)) {
                                pFS454Tbl[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pFS454Tbl[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pFS454Tbl[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                            else {
                                pFS454Tbl[n].CRTCNTSC2_8BPP[j] = *pRom;
                                pFS454Tbl[n].CRTCNTSC2_16BPP[j] = *pRom;
                                pFS454Tbl[n].CRTCNTSC2_32BPP[j] = *pRom++;
                                i++;
                            }
                        }
                    }
                }

            k = 3;
            /* LCK 3c5.44-45 */
                if (pViaModeTable->fs454MaskTable.misc2 & 0x18) {
                    pFS454Tbl[n].MiscNTSC2[k++] = *pRom++;
                    pFS454Tbl[n].MiscNTSC2[k++] = *pRom++;
                }

                /* Get RGB Table */
                pRom = pNTSC + 4;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pFS454Tbl[n].RGBNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pFS454Tbl[n].RGBNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

                /* Get YCbCr Table */
                pRom = pNTSC + 7;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pFS454Tbl[n].YCbCrNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pFS454Tbl[n].YCbCrNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

                /* Get DotCrawl Table */
                pRom = pNTSC + 10;
                pRom = pBIOS + *((CARD16 *)pRom);

                numReg = *pRom++;
                pFS454Tbl[n].DotCrawlNTSC[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pFS454Tbl[n].DotCrawlNTSC[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }
            }
        }
    }
}

void VIAGetTV3PAL(VIAModeTablePtr   pViaModeTable,
                    unsigned char   *pBIOS,
                    unsigned char   *pTable)
{
    VIABIOSTV3TablePtr  pTV3Tab;
    unsigned char       *pRom, *pPAL;
    unsigned char       *pTVTable;
    unsigned char       numCRTC;
    unsigned char       numPatch, numReg;
    int                 vScan, offset;
    int                 i, j, k, n;

    pTV3Tab = NULL;
    pTVTable = NULL;

    DEBUG(xf86Msg(X_INFO, "VIAGetTV3PAL\n"));
    for (vScan = 0; vScan < 2; vScan++) {
        switch (vScan) {
            case VIA_TVNORMAL:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_TVMODETAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV3Tab = pViaModeTable->tv3Table;
                /* HSoffset = 4; */
                break;
            case VIA_TVOVER:
                /* Get pointer table of all mode */
                pRom = pTable + VIA_BIOS_MODEOVERTAB_POS;
                pTVTable = pBIOS + *((CARD16 *)pRom);

                pTV3Tab = pViaModeTable->tv3OverTable;
                /* HSoffset = 40; */
                break;
        }

		/* offset: skip MODE3, MODE13 */
        for (offset = 13, n = 0; n < VIA_BIOS_NUM_TV3; offset += 5, n++) {

            /* Get pointer table of each mode */
            pRom = pTVTable + offset;

            if (*((CARD16 *)pRom) != 0) {

            pPAL = pBIOS + *((CARD16 *)pRom);

            /* Get start of TV Table */
            pRom = pPAL + 1;
            pRom = pBIOS + *((CARD16 *)pRom);

            for (i = 0, j = 0; i < pViaModeTable->tv3MaskTable.numTV; j++) {
                if (pViaModeTable->tv3MaskTable.TV[j] == 0xFF) {
                        pTV3Tab[n].TVPAL[j] = *pRom++;
                    i++;
                }
            }

            numCRTC = *pRom++;

            for (i = 0, j = 0, k = 0; i < numCRTC; j++) {
                if (pViaModeTable->tv3MaskTable.CRTC1[j] == 0xFF) {
                    if (i >= pViaModeTable->tv3MaskTable.numCRTC1) {
                            pTV3Tab[n].MiscPAL1[k++] = *pRom++;
                            i++;
                        }
                        else {
                            pTV3Tab[n].CRTCPAL1[j] = *pRom++;
                            i++;
                        }
                    }
                }

            for (i = 0, j = 0, k = 0; i < pViaModeTable->tv3MaskTable.numCRTC2; j++) {
                if (pViaModeTable->tv3MaskTable.CRTC2[j] == 0xFF) {
                    if (j == 0x15) {
                            pTV3Tab[n].CRTCPAL2_8BPP[j] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_8BPP[j+1] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_8BPP[j+2] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_16BPP[j] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_16BPP[j+1] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_16BPP[j+2] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_32BPP[j++] = *pRom++;
                            pTV3Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                        i += 3;
                    }
                    else {
                        /* For CRTC 6A-6C */
                        if (i >= (pViaModeTable->tv3MaskTable.numCRTC2 - 3)) {
                                pTV3Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                                pTV3Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                                pTV3Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                                i++;
                            }
                            else {
                                pTV3Tab[n].CRTCPAL2_8BPP[j] = *pRom;
                                pTV3Tab[n].CRTCPAL2_16BPP[j] = *pRom;
                                pTV3Tab[n].CRTCPAL2_32BPP[j] = *pRom++;
                                i++;
                            }
                        }
                    }
                }

            k = 3;
            /* LCK 3c5.44-45 */
            if (pViaModeTable->tv3MaskTable.misc2 & 0x18) {
                    pTV3Tab[n].MiscPAL2[k++] = *pRom++;
                    pTV3Tab[n].MiscPAL2[k++] = *pRom++;
            }

            /* Patch as setting 2nd path */
            numPatch = (int)(pViaModeTable->tv3MaskTable.misc2 >> 5);
            for (i = 0; i < numPatch; i++) {
                    pTV3Tab[n].PatchPAL2[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get RGB Table */
            pRom = pPAL + 4;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].RGBPAL[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].RGBPAL[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get YCbCr Table */
            pRom = pPAL + 7;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].YCbCrPAL[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].YCbCrPAL[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }

#if 0
            /* Get SDTV_RGB Table */
            pRom = pPAL + 10;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].SDTV_RGBPAL[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].SDTV_RGBPAL[i] = *((CARD16 *)pRom);
                pRom += 2;
            }

            /* Get SDTV_YCbCr Table */
            pRom = pPAL + 13;
            pRom = pBIOS + *((CARD16 *)pRom);

            numReg = *pRom++;
                pTV3Tab[n].SDTV_YCbCrPAL[0] = numReg;
                for ( i = 1; i < (numReg + 1); i++) {
                    pTV3Tab[n].SDTV_YCbCrPAL[i] = *((CARD16 *)pRom);
                    pRom += 2;
                }
#endif
            }
        }
    }
}


/* Check TV mode table is TV2+ or TV3 */
int VIAGetTVTabVer(VIABIOSInfoPtr   pBIOSInfo,
                    unsigned char   *pBIOS)
{
    unsigned char       *pRom, *pTable;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAGetTVTabVer\n"));
    /* Get the start of Table */
    pRom = pBIOS + VIA_BIOS_OFFSET_POS;
    pTable = pBIOS + *((CARD16 *)pRom);

    /* Get start of TV Mask Table */
    pRom = pTable + VIA_BIOS_TVMASKTAB_POS;
    pRom = pBIOS + *((CARD16 *)pRom);

    pRom += 12;
    if (*((CARD16 *)pRom) == 0x03) {
        pBIOSInfo->BIOSTVTabVer = 2;
        return 2;
    }
    else {
        pBIOSInfo->BIOSTVTabVer = 3;
        return 3;
    }
}


Bool VIAGetBIOSTable(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    unsigned char   *pBIOS = NULL, *pRom, *pTable, *pFPanel, *pLCDTable, *pSuptTable;
    unsigned char   *pRefreshTableStart;
    unsigned char   *pRefreshIndexTable;
    unsigned char   *pRefreshTable;
    unsigned char   numSuptPanel, numEntry;
    unsigned short  tableSize;
    int             romSize;
    int             i = 0, j, k, m, sum;


    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAGetBIOSTable\n"));

    if (!(pBIOS=xcalloc(1, 0x10000)))  {
        ErrorF("Allocate memory fail !!\n");
        return FALSE;
    }

    if (xf86ReadBIOS(0xC0000, 0, pBIOS, 0x10000) != 0x10000)  {
        pBIOSInfo->UseBIOS = FALSE;
        xfree(pBIOS);
        ErrorF("Read VGA BIOS image fail !!\n");
    }
    else {
        if (*((CARD16 *) pBIOS) != 0xAA55) {
            pBIOSInfo->UseBIOS = FALSE;
            xfree(pBIOS);
            ErrorF("VGA BIOS image is wrong!!\n");
        }
        else {
            romSize = *((CARD8 *) (pBIOS + VIA_BIOS_SIZE_POS)) * 512;
            pRom = pBIOS;
            sum = 0;

            for (i = 0; i < romSize; i++) {
                sum += *pRom++;
            }

            if (((CARD8) sum) != 0) {
                pBIOSInfo->UseBIOS = FALSE;
                xfree(pBIOS);
                ErrorF("VGA BIOS image is wrong!! CheckSum = %x\n", sum);
            }
        }
    }

    /* To check is TV Encoder match with BIOS TV mode table */
    if (pBIOSInfo->UseBIOS) {
        int TVEncoder = pBIOSInfo->TVEncoder;
        if (TVEncoder && (VIAGetTVTabVer(pBIOSInfo, pBIOS) != (TVEncoder+1))) {
            pBIOSInfo->UseBIOS = FALSE;
            xfree(pBIOS);
        }
    }

    /* Get the start of Table */
    pRom = pBIOS + VIA_BIOS_OFFSET_POS;
    pTable = pBIOS + *((CARD16 *)pRom);

#ifdef CREATE_CH7019_HEADERFILE
    VIAGetCH7019Mask(pViaModeTable, pBIOS, pTable);
    VIAGetCH7019NTSC(pViaModeTable, pBIOS, pTable);
    VIAGetCH7019PAL(pViaModeTable, pBIOS, pTable);
    VIACreateCH7019(pViaModeTable);
#else
    pViaModeTable->ch7019MaskTable = ch7019MaskTable;
    for (i = 0; i < VIA_BIOS_NUM_CH7019; i++) {
        pViaModeTable->ch7019Table[i] = ch7019Table[i];
        pViaModeTable->ch7019OverTable[i] = ch7019OverTable[i];
    }
#endif
#ifdef CREATE_FS454_HEADERFILE
    VIAGetFS454Mask(pViaModeTable, pBIOS, pTable);
    VIAGetFS454NTSC(pViaModeTable, pBIOS, pTable);
    /*VIAGetFS454PAL(pViaModeTable, pBIOS, pTable);*/
	VIACreateFS454(pViaModeTable);
#else
    pViaModeTable->fs454MaskTable = fs454MaskTable;
    for (i = 0; i < VIA_BIOS_NUM_FS454; i++) {
        pViaModeTable->fs454Table[i] = fs454Table[i];
        pViaModeTable->fs454OverTable[i] = fs454OverTable[i];
    }
#endif
#ifdef CREATE_SAA7108_HEADERFILE
    VIAGetSAA7108Mask(pViaModeTable, pBIOS, pTable);
    VIAGetSAA7108NTSC(pViaModeTable, pBIOS, pTable);
    VIAGetSAA7108PAL(pViaModeTable, pBIOS, pTable);
    VIACreateSAA7108(pViaModeTable);
#else
    pViaModeTable->saa7108MaskTable = saa7108MaskTable;
    for (i = 0; i < VIA_BIOS_NUM_SAA7108; i++) {
        pViaModeTable->saa7108Table[i] = saa7108Table[i];
        pViaModeTable->saa7108OverTable[i] = saa7108OverTable[i];
    }
#endif
#ifdef CREATE_TV2_HEADERFILE
    VIAGetTV2Mask(pViaModeTable, pBIOS, pTable);
    VIAGetTV2NTSC(pViaModeTable, pBIOS, pTable);
    VIAGetTV2PAL(pViaModeTable, pBIOS, pTable);
    VIACreateTV2(pViaModeTable);
#else
    pViaModeTable->tv2MaskTable = tv2MaskTable;
    for (i = 0; i < VIA_BIOS_NUM_TV2; i++) {
        pViaModeTable->tv2Table[i] = tv2Table[i];
        pViaModeTable->tv2OverTable[i] = tv2OverTable[i];
    }
#endif
#ifdef CREATE_TV3_HEADERFILE
    VIAGetTV3Mask(pViaModeTable, pBIOS, pTable);
    VIAGetTV3NTSC(pViaModeTable, pBIOS, pTable);
    VIAGetTV3PAL(pViaModeTable, pBIOS, pTable);
    VIACreateTV3(pViaModeTable);
#else
    pViaModeTable->tv3MaskTable = tv3MaskTable;
    for (i = 0; i < VIA_BIOS_NUM_TV3; i++) {
        pViaModeTable->tv3Table[i] = tv3Table[i];
        pViaModeTable->tv3OverTable[i] = tv3OverTable[i];
    }
#endif
#ifdef CREATE_VT1622A_HEADERFILE
    VIAGetTV3Mask(pViaModeTable, pBIOS, pTable);
    VIAGetTV3NTSC(pViaModeTable, pBIOS, pTable);
    VIAGetTV3PAL(pViaModeTable, pBIOS, pTable);
    VIACreateVT1622A(pViaModeTable);
#else
    pViaModeTable->vt1622aMaskTable = vt1622aMaskTable;
    for (i = 0; i < VIA_BIOS_NUM_TV3; i++) {
        pViaModeTable->vt1622aTable[i] = vt1622aTable[i];
        pViaModeTable->vt1622aOverTable[i] = vt1622aOverTable[i];
    }
#endif
#ifndef CREATE_MODETABLE_HEADERFILE
    if (!pBIOSInfo->UseBIOS) {
        pViaModeTable->BIOSVer = BIOSVer;
        pViaModeTable->BIOSDate = BIOSDate;
        pViaModeTable->NumModes = NumModes;
        pViaModeTable->NumPowerOn = NumPowerOn;
        pViaModeTable->NumPowerOff = NumPowerOff;
        pViaModeTable->Modes = Modes;
        pViaModeTable->commExtTable = commExtTable;
        pViaModeTable->stdModeExtTable = stdModeExtTable;
        for (i = 0; i < VIA_BIOS_NUM_RES; i++) {
            for (j = 0; j < VIA_BIOS_NUM_REFRESH; j++) {
                pViaModeTable->refreshTable[i][j] = refreshTable[i][j];
            }
        }
        for (i = 0; i < VIA_BIOS_NUM_PANEL; i++) {
            pViaModeTable->lcdTable[i] = lcdTable[i];
        }
        for (i = 0; i < VIA_BIOS_NUM_LCD_POWER_SEQ; i++) {
            pViaModeTable->powerOn[i] = powerOn[i];
            pViaModeTable->powerOff[i] = powerOff[i];
        }
        pViaModeTable->modeFix = modeFix;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "BIOS Version: %x.%x\n",
                (pViaModeTable->BIOSVer >> 8), (pViaModeTable->BIOSVer & 0xFF)));
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "BIOS Release Date: %s\n", pViaModeTable->BIOSDate));
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "Get mode table from via_mode.h\n"));
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "-- VIAGetBIOSTable Done!\n"));
        xfree(pBIOS);
        return TRUE;
    }
#else
    pBIOSInfo->UseBIOS = TRUE;
#endif /* #ifndef CREATE_MODETABLE_HEADERFILE */

    /* Get the start of biosver structure */
    pRom = pTable + VIA_BIOS_BIOSVER_POS;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "bcpPost: %X\n", i, *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    /* The offset should be 44, but the actual image is less three char. */
    /* pRom += 44; */
    pRom += 41;
    pViaModeTable->BIOSVer = *pRom++;
    pViaModeTable->BIOSVer = (pViaModeTable->BIOSVer << 8) | *pRom;
    xf86DrvMsg(pBIOSInfo->scrnIndex, X_DEFAULT, "BIOS Version: %x.%x\n",
            (pViaModeTable->BIOSVer >> 8), (pViaModeTable->BIOSVer & 0xFF));

    /* Get the start of bcpPost structure */
    pRom = pTable + VIA_BIOS_BCPPOST_POS;
    pRom = pBIOS + *((CARD16 *)pRom);

    pViaModeTable->BIOSDate = (char *) xcalloc(9, sizeof(char));

    pRom += 10;
    for (i = 0; i < 8; i++) {
        pViaModeTable->BIOSDate[i] = *pRom;
        pRom++;
    }
    xf86DrvMsg(pBIOSInfo->scrnIndex, X_DEFAULT, "BIOS Release Date: %s\n", pViaModeTable->BIOSDate);

    /* Get CSTAB Tables */
    pRom = pTable + VIA_BIOS_CSTAB_POS;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "cstabExtendEnd: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    pViaModeTable->NumModes = *pRom;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "BIOS Support Mode Numbers: %hu\n", pViaModeTable->NumModes));
    pViaModeTable->Modes = (VIAModeEntryPtr) xcalloc(pViaModeTable->NumModes,
                                                   sizeof(VIAModeEntry));

    for (i = 0, j = pViaModeTable->NumModes - 1; i < pViaModeTable->NumModes; i++, j--) {
        pRom -= 8;
        pViaModeTable->Modes[j].Mode = *pRom;
        pViaModeTable->Modes[j].Bpp = *(pRom + 2);
        pViaModeTable->Modes[j].extModeExtTable.numEntry = 0;
        pViaModeTable->Modes[j].MClk = 0;
        pViaModeTable->Modes[j].VClk = 0;

        /* Using Mode Number To Set Resolution */
        switch (pViaModeTable->Modes[j].Mode) {
            case 0x14:
            case 0x15:
            case 0x16:
                pViaModeTable->Modes[j].Height = 576;
                pViaModeTable->Modes[j].Width = 1024;
                break;
            case 0x22:
            case 0x23:
            case 0x24:
                pViaModeTable->Modes[j].Height = 300;
                pViaModeTable->Modes[j].Width = 400;
                break;
            case 0x25:
            case 0x26:
            case 0x27:
                pViaModeTable->Modes[j].Height = 384;
                pViaModeTable->Modes[j].Width = 512;
                break;
            case 0x2E:
            case 0x2F:
            case 0x30:
                pViaModeTable->Modes[j].Height = 400;
                pViaModeTable->Modes[j].Width = 640;
                break;
            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
                pViaModeTable->Modes[j].Height = 480;
                pViaModeTable->Modes[j].Width = 640;
                break;
            case 0x35:
            case 0x36:
            case 0x37:
            case 0x38:
            case 0x39:
                pViaModeTable->Modes[j].Height = 600;
                pViaModeTable->Modes[j].Width = 800;
                break;
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E:
                pViaModeTable->Modes[j].Height = 768;
                pViaModeTable->Modes[j].Width = 1024;
                break;
            case 0x3F:
            case 0x40:
            case 0x41:
            case 0x42:
            case 0x43:
                pViaModeTable->Modes[j].Height = 864;
                pViaModeTable->Modes[j].Width = 1152;
                break;
            case 0x44:
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x48:
                pViaModeTable->Modes[j].Height = 1024;
                pViaModeTable->Modes[j].Width = 1280;
                break;
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
            case 0x4D:
                pViaModeTable->Modes[j].Height = 1200;
                pViaModeTable->Modes[j].Width = 1600;
                break;
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
                pViaModeTable->Modes[j].Height = 1050;
                pViaModeTable->Modes[j].Width = 1440;
                break;
            case 0x54:
            case 0x55:
            case 0x56:
            case 0x57:
                pViaModeTable->Modes[j].Height = 768;
                pViaModeTable->Modes[j].Width = 1280;
                break;
            case 0x58:
            case 0x59:
            case 0x5A:
            case 0x5B:
                pViaModeTable->Modes[j].Height = 960;
                pViaModeTable->Modes[j].Width = 1280;
                break;
#if 0
            case 0x60:
            case 0x61:
            case 0x62:
                pViaModeTable->Modes[j].Height = 1440;
                pViaModeTable->Modes[j].Width = 1920;
                break;
#endif
            case 0x63:
            case 0x64:
            case 0x65:
                pViaModeTable->Modes[j].Height = 480;
                pViaModeTable->Modes[j].Width = 848;
                break;
            case 0x66:
            case 0x67:
            case 0x68:
                pViaModeTable->Modes[j].Height = 1050;
                pViaModeTable->Modes[j].Width = 1400;
                break;
            case 0x70:
            case 0x71:
            case 0x72:
                pViaModeTable->Modes[j].Height = 480;
                pViaModeTable->Modes[j].Width = 720;
                break;
            case 0x73:
            case 0x74:
            case 0x75:
                pViaModeTable->Modes[j].Height = 576;
                pViaModeTable->Modes[j].Width = 720;
                break;
            case 0x76:
            case 0x77:
            case 0x78:
                pViaModeTable->Modes[j].Height = 512;
                pViaModeTable->Modes[j].Width = 1024;
                break;
            case 0x79:
            case 0x7A:
            case 0x7B:
                pViaModeTable->Modes[j].Height = 480;
                pViaModeTable->Modes[j].Width = 856;
                break;
            case 0x5C:
            case 0x5D:
            case 0x5E:
                pViaModeTable->Modes[j].Height = 200;
                pViaModeTable->Modes[j].Width = 320;
                break;
            case 0x7C:
            case 0x7D:
            case 0x7E:
                pViaModeTable->Modes[j].Height = 240;
                pViaModeTable->Modes[j].Width = 320;
                break;
            default:
                pViaModeTable->Modes[j].Height = 0;
                pViaModeTable->Modes[j].Width = 0;
                break;
        }
    }

    /* Get Set Mode Regs. Init. (Standard VGA) */
    pRom = pTable + VIA_BIOS_STDVGAREGTAB_POS;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "csVidParams: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    for (i = 0; i < pViaModeTable->NumModes; i++) {
        pViaModeTable->Modes[i].stdVgaTable.columns = *pRom++;
        pViaModeTable->Modes[i].stdVgaTable.rows = *pRom++;
        pViaModeTable->Modes[i].stdVgaTable.fontHeight = *pRom++;
        pViaModeTable->Modes[i].stdVgaTable.pageSize = *((CARD16 *)pRom++);

        pRom++;

        for (j = 1; j < 5; j++) {
            pViaModeTable->Modes[i].stdVgaTable.SR[j] = *pRom++;
        }

        pViaModeTable->Modes[i].stdVgaTable.misc = *pRom++;

        for (j = 0; j < 25; j++) {
            pViaModeTable->Modes[i].stdVgaTable.CR[j] = *pRom++;
        }

        for (j = 0; j < 20; j++) {
            pViaModeTable->Modes[i].stdVgaTable.AR[j] = *pRom++;
        }

        for (j = 0; j < 9; j++) {
            pViaModeTable->Modes[i].stdVgaTable.GR[j] = *pRom++;
        }
    }

    /* Get Commmon Ext. Regs */
    pRom = pTable + VIA_BIOS_COMMEXTREGTAB_POS;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "Mode_XRegs: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    for (j = 0; *pRom != 0xFF; j++) {
        switch (*pRom++) {
            case ZCR:
                pViaModeTable->commExtTable.port[j] = 0xD4;
                break;
            case ZSR:
                pViaModeTable->commExtTable.port[j] = 0xC4;
                break;
            case ZGR:
                pViaModeTable->commExtTable.port[j] = 0xCE;
                break;
            default:
                pViaModeTable->commExtTable.port[j] = *pRom;
                break;
        }
        /*pViaModeTable->commExtTable.port[j] = *pRom++;*/
        pViaModeTable->commExtTable.offset[j] = *pRom++;
        pViaModeTable->commExtTable.mask[j] = *pRom++;
        pViaModeTable->commExtTable.data[j] = *pRom++;
    }

    pViaModeTable->commExtTable.numEntry = j;

    /* Get Standard Mode-Spec. Extend Regs */
    pRom = pTable + VIA_BIOS_STDMODEXTREGTAB_POS;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "StdMode_XRegs: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    for (j = 0; *pRom != 0xFF; j++) {
        switch (*pRom++) {
            case ZCR:
                pViaModeTable->stdModeExtTable.port[j] = 0xD4;
                break;
            case ZSR:
                pViaModeTable->stdModeExtTable.port[j] = 0xC4;
                break;
            case ZGR:
                pViaModeTable->stdModeExtTable.port[j] = 0xCE;
                break;
            default:
                pViaModeTable->stdModeExtTable.port[j] = *pRom;
                break;
        }
        /*pViaModeTable->stdModeExtTable.port[j] = *pRom++;*/
        pViaModeTable->stdModeExtTable.offset[j] = *pRom++;
        pViaModeTable->stdModeExtTable.mask[j] = *pRom++;
        pViaModeTable->stdModeExtTable.data[j] = *pRom++;
    }

    pViaModeTable->stdModeExtTable.numEntry = j;

    /* Get Extended Mode-Spec. Extend Regs */
    pRom = pTable + VIA_BIOS_EXTMODEREGTAB_POS;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "csextModeTbl: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    while (*pRom != 0xFF) {
        unsigned char   *pExtModeExtTable;
        CARD16          mode = *pRom;

        for (i = 0; i < pViaModeTable->NumModes; i++) {
            if (pViaModeTable->Modes[i].Mode == mode)
                break;
        }

        if (i == pViaModeTable->NumModes) {
            /* Cannot find a match mode, skip this mode information */
            pRom += 11;
            continue;
        }

        pViaModeTable->Modes[i].MemNeed = *(pRom + 1);
        pViaModeTable->Modes[i].VClk = *((CARD16 *)(pRom + 2));
        pViaModeTable->Modes[i].MClk = *((CARD16 *)(pRom + 4));

        pExtModeExtTable = pBIOS + *((CARD16 *)(pRom + 6));

        for (j = 0; *pExtModeExtTable != 0xFF; j++) {
            switch (*pExtModeExtTable++) {
                case ZCR:
                    pViaModeTable->Modes[i].extModeExtTable.port[j] = 0xD4;
                    break;
                case ZSR:
                    pViaModeTable->Modes[i].extModeExtTable.port[j] = 0xC4;
                    break;
                case ZGR:
                    pViaModeTable->Modes[i].extModeExtTable.port[j] = 0xCE;
                    break;
                default:
                    pViaModeTable->Modes[i].extModeExtTable.port[j] = *pExtModeExtTable;
                    break;
            }
            /*pViaModeTable->Modes[i].extModeExtTable.port[j] = *pExtModeExtTable++;*/
            pViaModeTable->Modes[i].extModeExtTable.offset[j] = *pExtModeExtTable++;
            pViaModeTable->Modes[i].extModeExtTable.mask[j] = *pExtModeExtTable++;
            pViaModeTable->Modes[i].extModeExtTable.data[j] = *pExtModeExtTable++;
        }

        pViaModeTable->Modes[i].extModeExtTable.numEntry = j;

        pRom += 11;
    }

    /* Get Refresh Rate Table */
    pRom = pTable + VIA_BIOS_REFRESHMODETAB_POS;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "csModeRateTbl: %X\n", *((CARD16 *)pRom)));
    pRom = pBIOS + *((CARD16 *)pRom);

    for (i = 0; i < VIA_BIOS_NUM_RES; i++) {
        for (j = 0; j < VIA_BIOS_NUM_REFRESH; j++) {
            /* Set all modes is invalid */
            pViaModeTable->refreshTable[i][j].refresh = 0;
        }
    }

    i = 0;

    while (*((CARD16 *)pRom) != 0xFFFF) {
        if (i >= VIA_BIOS_NUM_RES) {
            xfree(pBIOS);
            xfree(pViaModeTable->BIOSDate);
            xfree(pViaModeTable->Modes);
            ErrorF("Too many modes for Refresh Table!!\n");
            return FALSE;
        }

        pRefreshTableStart = pBIOS + *((CARD16 *)pRom);
        pRefreshIndexTable = pRefreshTableStart + 3;
        j = 0;

        while (*pRefreshIndexTable != 0xFF) {
            if (j >= VIA_BIOS_NUM_REFRESH) {
                xfree(pBIOS);
                xfree(pViaModeTable->BIOSDate);
                xfree(pViaModeTable->Modes);
                ErrorF("Too many refresh modes for Refresh Table!!\n");
                return FALSE;
            }

            switch (*pRefreshIndexTable) {
                case 0:
                    pViaModeTable->refreshTable[i][j].refresh = 60;
                    break;
                case 1:
                    pViaModeTable->refreshTable[i][j].refresh = 56;
                    break;
                case 2:
                    pViaModeTable->refreshTable[i][j].refresh = 65;
                    break;
                case 3:
                    pViaModeTable->refreshTable[i][j].refresh = 70;
                    break;
                case 4:
                    pViaModeTable->refreshTable[i][j].refresh = 72;
                    break;
                case 5:
                    pViaModeTable->refreshTable[i][j].refresh = 75;
                    break;
                case 6:
                    pViaModeTable->refreshTable[i][j].refresh = 80;
                    break;
                case 7:
                    pViaModeTable->refreshTable[i][j].refresh = 85;
                    break;
                case 8:
                    pViaModeTable->refreshTable[i][j].refresh = 90;
                    break;
                case 9:
                    pViaModeTable->refreshTable[i][j].refresh = 100;
                    break;
                case 10:
                    pViaModeTable->refreshTable[i][j].refresh = 120;
                    break;
                default:
                    pViaModeTable->refreshTable[i][j].refresh = 255;
                    break;
            }

            pRefreshTable = pRefreshTableStart + *(pRefreshIndexTable + 1);

            pViaModeTable->refreshTable[i][j].VClk = *((CARD16 *) pRefreshTable);
            pRefreshTable += 2;

            for (k = 0; k < 11; k++) {
                pViaModeTable->refreshTable[i][j].CR[k] = *pRefreshTable++;
            }

            pRefreshIndexTable += 3;
            j++;
        }

        pRom += 2;

        /* Skip Mode table, we don't need it */
        while (*pRom++ != 0x0)
            ;

        i++;
    }

    /* Get BIOS LCD Mode Table */
    /* Get start of LCD Table */
    pRom = pTable + VIA_BIOS_LCDMODETAB_POS;
    pRom = pBIOS + *((CARD16 *)pRom);

    /* No. of Support Panels */
    pRom += 6; /* Skip six char. - "FPANEL" */
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "numSuptPanel: %X\n", *pRom));
    numSuptPanel = *pRom;

    /* Point to LCD 640x480 Table */
    pRom += 3;
    pFPanel = pRom;

    for (i = 0; i < numSuptPanel; i++) {

        /* Get fpIndex */
        pViaModeTable->lcdTable[i].fpIndex = *pRom++;

        /* Get fpSize */
        pViaModeTable->lcdTable[i].fpSize = *pRom++;

        /* Get No. of Entry */
        numEntry = *pRom++;

        /* vidMemAdjust skip */
        pRom++;

        /* Get Table Size */
        tableSize = *((CARD16 *)pRom);

        /* Get Support Mode Table */
        pRom += 2;
        pSuptTable = pRom;

        /* Get Power Seqence Index */
        pRom += 2;
        pViaModeTable->lcdTable[i].powerSeq = *pRom++;

        pLCDTable = pRom;

        pRom = pSuptTable;
        pRom = pBIOS + *((CARD16 *)pRom);

        for (j = 0; j < VIA_BIOS_NUM_LCD_SUPPORT_MASK; j++) {
            pViaModeTable->lcdTable[i].SuptMode[j] = *((CARD16 *)pRom);
            pRom += 2;
        }

        /* Get FPconfig Table */
        pRom = pLCDTable;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "FPconfigTbl000%d: %X\n", i, *((CARD16 *)pRom)));
        pRom = pBIOS + *((CARD16 *)pRom);

        for (j = 0; *pRom != 0xFF; j++) {
            switch (*pRom++) {
                case ZCR:
                    pViaModeTable->lcdTable[i].FPconfigTb.port[j] = 0xD4;
                    break;
                case ZSR:
                    pViaModeTable->lcdTable[i].FPconfigTb.port[j] = 0xC4;
                    break;
                case ZGR:
                    pViaModeTable->lcdTable[i].FPconfigTb.port[j] = 0xCE;
                    break;
                default:
                    pViaModeTable->lcdTable[i].FPconfigTb.port[j] = *pRom;
                    break;
            }
            /*pViaModeTable->lcdTable[i].FPconfigTb.port[j] = *pRom++;*/
            pViaModeTable->lcdTable[i].FPconfigTb.offset[j] = *pRom++;
            pViaModeTable->lcdTable[i].FPconfigTb.data[j] = *pRom++;
        }
        pViaModeTable->lcdTable[i].FPconfigTb.numEntry = j;

        /* Get Init Table */
        pRom = pLCDTable + 2;
        pRom = pBIOS + *((CARD16 *)pRom);

        pViaModeTable->lcdTable[i].InitTb.LCDClk = *((CARD16 *)pRom);
        pViaModeTable->lcdTable[i].InitTb.VClk = *((CARD16 *)(pRom+2));
        pRom += 4;

        pViaModeTable->lcdTable[i].InitTb.LCDClk_12Bit = *((CARD16 *)pRom);
        pViaModeTable->lcdTable[i].InitTb.VClk_12Bit = *((CARD16 *)(pRom+2));
        pRom += 4;

        for (j = 0; *pRom != 0xFF; j++) {
            /*pViaModeTable->lcdTable[i].InitTb.port[j] = *pRom++;*/
            pViaModeTable->lcdTable[i].InitTb.port[j] = 0xD4;
            pViaModeTable->lcdTable[i].InitTb.offset[j] = *pRom++;
            pViaModeTable->lcdTable[i].InitTb.data[j] = *pRom++;
        }
        pViaModeTable->lcdTable[i].InitTb.numEntry = j;

        /* Get MPatchDP2Ctr Table */
        pRom = pLCDTable + 4;
        pRom = pBIOS + *((CARD16 *)pRom);

        for (j = 0; *pRom != 0xFF; j++) {
            pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].Mode = *pRom;
            pRom++;

            pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].LCDClk = *((CARD16 *)pRom);
            pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].VClk = *((CARD16 *)(pRom+2));
            pRom += 4;

            pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].LCDClk_12Bit = *((CARD16 *)pRom);
            pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].VClk_12Bit = *((CARD16 *)(pRom+2));
            pRom += 4;
            for (k = 0; *pRom != 0xFF; k++) {
                pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].port[k] = 0xD4;
                pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].offset[k] = *pRom++;
                pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].data[k] = *pRom++;
            }
            pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].numEntry = k;
            pRom++;
        }
        pViaModeTable->lcdTable[i].numMPatchDP2Ctr = j;

        /* Get MPatchDP2Exp Table */
        pRom = pLCDTable + 6;
        pRom = pBIOS + *((CARD16 *)pRom);

        for (j = 0; *pRom != 0xFF; j++) {
            pViaModeTable->lcdTable[i].MPatchDP2Exp[j].Mode = *pRom;
            pRom++;

            pViaModeTable->lcdTable[i].MPatchDP2Exp[j].LCDClk = *((CARD16 *)pRom);
            pViaModeTable->lcdTable[i].MPatchDP2Exp[j].VClk = *((CARD16 *)(pRom+2));
            pRom += 4;

            pViaModeTable->lcdTable[i].MPatchDP2Exp[j].LCDClk_12Bit = *((CARD16 *)pRom);
            pViaModeTable->lcdTable[i].MPatchDP2Exp[j].VClk_12Bit = *((CARD16 *)(pRom+2));
            pRom += 4;
            for (k = 0; *pRom != 0xFF; k++) {
                pViaModeTable->lcdTable[i].MPatchDP2Exp[j].port[k] = 0xD4;
                pViaModeTable->lcdTable[i].MPatchDP2Exp[j].offset[k] = *pRom++;
                pViaModeTable->lcdTable[i].MPatchDP2Exp[j].data[k] = *pRom++;
            }
            pViaModeTable->lcdTable[i].MPatchDP2Exp[j].numEntry = k;
            pRom++;
        }
        pViaModeTable->lcdTable[i].numMPatchDP2Exp = j;

        /* Get MPatchDP1Ctr Table */
        pRom = pLCDTable + 8;
        pRom = pBIOS + *((CARD16 *)pRom);

        for (j = 0; *pRom != 0xFF; j++) {
            pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].Mode = *pRom;
            pRom++;

            for (k = 0; *pRom != 0xFF; k++) {
                switch (*pRom++) {
                    case ZCR:
                        pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].port[k] = 0xD4;
                        break;
                    case ZSR:
                        pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].port[k] = 0xC4;
                        break;
                    case ZGR:
                        pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].port[k] = 0xCE;
                        break;
                    default:
                        pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].port[k] = *pRom;
                        break;
                }
                /*pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].port[k] = *pRom++;*/
                pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].offset[k] = *pRom++;
                pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].data[k] = *pRom++;
            }
            pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].numEntry = k;
            pRom++;
        }
        pViaModeTable->lcdTable[i].numMPatchDP1Ctr = j;

        /* Get MPatchDP1Exp Table */
        pRom = pLCDTable + 10;
        pRom = pBIOS + *((CARD16 *)pRom);

        for (j = 0; *pRom != 0xFF; j++) {
            pViaModeTable->lcdTable[i].MPatchDP1Exp[j].Mode = *pRom;
            pRom++;

            for (k = 0; *pRom != 0xFF; k++) {
                switch (*pRom++) {
                    case ZCR:
                        pViaModeTable->lcdTable[i].MPatchDP1Exp[j].port[k] = 0xD4;
                        break;
                    case ZSR:
                        pViaModeTable->lcdTable[i].MPatchDP1Exp[j].port[k] = 0xC4;
                        break;
                    case ZGR:
                        pViaModeTable->lcdTable[i].MPatchDP1Exp[j].port[k] = 0xCE;
                        break;
                    default:
                        pViaModeTable->lcdTable[i].MPatchDP1Exp[j].port[k] = *pRom;
                        break;
                }
                /*pViaModeTable->lcdTable[i].MPatchDP1Exp[j].port[k] = *pRom++;*/
                pViaModeTable->lcdTable[i].MPatchDP1Exp[j].offset[k] = *pRom++;
                pViaModeTable->lcdTable[i].MPatchDP1Exp[j].data[k] = *pRom++;
            }
            pViaModeTable->lcdTable[i].MPatchDP1Exp[j].numEntry = k;
            pRom++;
        }
        pViaModeTable->lcdTable[i].numMPatchDP1Exp = j;

        /* Get LowResCtr Table */
        pRom = pLCDTable + 12;
        pRom = pBIOS + *((CARD16 *)pRom);

        pViaModeTable->lcdTable[i].LowResCtr.LCDClk = *((CARD16 *)pRom);
        pViaModeTable->lcdTable[i].LowResCtr.VClk = *((CARD16 *)(pRom+2));
        pRom += 4;

        pViaModeTable->lcdTable[i].LowResCtr.LCDClk_12Bit = *((CARD16 *)pRom);
        pViaModeTable->lcdTable[i].LowResCtr.VClk_12Bit = *((CARD16 *)(pRom+2));
        pRom += 4;

        for (j = 0; *pRom != 0xFF; j++) {
            /*pViaModeTable->lcdTable[i].LowResCtr.port[j] = *pRom++;*/
            pViaModeTable->lcdTable[i].LowResCtr.port[j] = 0xD4;
            pViaModeTable->lcdTable[i].LowResCtr.offset[j] = *pRom++;
            pViaModeTable->lcdTable[i].LowResCtr.data[j] = *pRom++;
        }
        pViaModeTable->lcdTable[i].LowResCtr.numEntry = j;

        /* Get LowResExp Table */
        pRom = pLCDTable + 14;
        pRom = pBIOS + *((CARD16 *)pRom);

        pViaModeTable->lcdTable[i].LowResExp.LCDClk = *((CARD16 *)pRom);
        pViaModeTable->lcdTable[i].LowResExp.VClk = *((CARD16 *)(pRom+2));
        pRom += 4;

        pViaModeTable->lcdTable[i].LowResExp.LCDClk_12Bit = *((CARD16 *)pRom);
        pViaModeTable->lcdTable[i].LowResExp.VClk_12Bit = *((CARD16 *)(pRom+2));
        pRom += 4;

        for (j = 0; *pRom != 0xFF; j++) {
            /*pViaModeTable->lcdTable[i].LowResExp.port[j] = *pRom++;*/
            pViaModeTable->lcdTable[i].LowResExp.port[j] = 0xD4;
            pViaModeTable->lcdTable[i].LowResExp.offset[j] = *pRom++;
            pViaModeTable->lcdTable[i].LowResExp.data[j] = *pRom++;
        }
        pViaModeTable->lcdTable[i].LowResExp.numEntry = j;

        /* No. of Mxxx */
        numEntry = (numEntry - 8) / 2;

        /* Get MxxxCtr & MxxxExp Table */
        for (j = 0, m = 8; j < numEntry; j++, m++) {
            pRom = pLCDTable + (m * 2);
            pRom = pBIOS + *((CARD16 *)pRom);

            if (*pRom == 0xFF) {
                k = 0;
            }
            else {
                pViaModeTable->lcdTable[i].MCtr[j].LCDClk = *((CARD16 *)pRom);
                pViaModeTable->lcdTable[i].MCtr[j].VClk = *((CARD16 *)(pRom+2));
                pRom += 4;

                pViaModeTable->lcdTable[i].MCtr[j].LCDClk_12Bit = *((CARD16 *)pRom);
                pViaModeTable->lcdTable[i].MCtr[j].VClk_12Bit = *((CARD16 *)(pRom+2));
                pRom += 4;

                for (k = 0; *pRom != 0xFF; k++) {
                    /*pViaModeTable->lcdTable[i].MCtr[j].port[k] = *pRom++;*/
                    pViaModeTable->lcdTable[i].MCtr[j].port[k] = 0xD4;
                    pViaModeTable->lcdTable[i].MCtr[j].offset[k] = *pRom++;
                    pViaModeTable->lcdTable[i].MCtr[j].data[k] = *pRom++;
                }
            }
            pViaModeTable->lcdTable[i].MCtr[j].numEntry = k;

            m++;
            pRom = pLCDTable + (m * 2);
            pRom = pBIOS + *((CARD16 *)pRom);

            if (*pRom == 0xFF) {
                k = 0;
            }
            else {
                pViaModeTable->lcdTable[i].MExp[j].LCDClk = *((CARD16 *)pRom);
                pViaModeTable->lcdTable[i].MExp[j].VClk = *((CARD16 *)(pRom+2));
                pRom += 4;

                pViaModeTable->lcdTable[i].MExp[j].LCDClk_12Bit = *((CARD16 *)pRom);
                pViaModeTable->lcdTable[i].MExp[j].VClk_12Bit = *((CARD16 *)(pRom+2));
                pRom += 4;

                for (k = 0; *pRom != 0xFF; k++) {
                    /*pViaModeTable->lcdTable[i].MExp[j].port[k] = *pRom++;*/
                    pViaModeTable->lcdTable[i].MExp[j].port[k] = 0xD4;
                    pViaModeTable->lcdTable[i].MExp[j].offset[k] = *pRom++;
                    pViaModeTable->lcdTable[i].MExp[j].data[k] = *pRom++;
                }
            }
            pViaModeTable->lcdTable[i].MExp[j].numEntry = k;
        }

        /* Point to Next Support Panel */
        pRom = pFPanel + tableSize;
        pFPanel = pRom;
    }

    /* Get start of PowerOn Seqence Table */
    if (VIAGetTVTabVer(pBIOSInfo, pBIOS) == 3) {
        pRom = pTable + VIA_BIOS_LCDPOWERON_POS;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "LCD_POWER_ON: %X\n", *((CARD16 *)pRom)));
        pRom = pBIOS + *((CARD16 *)pRom);
    }
    else {
        pRom = pTable + VIA_BIOS_LCDPOWERON_POS - 2;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "LCD_POWER_ON: %X\n", *((CARD16 *)pRom)));
        pRom = pBIOS + *((CARD16 *)pRom);
    }

    for (i = 0; (*((CARD16 *)pRom)) != 0xFFFF; i++) {
        if ((*pRom) == 0xFF)
            pRom++;

        pViaModeTable->powerOn[i].powerSeq = *pRom++;

        pRom += 2;

        for (j = 0; *pRom != 0xFF; j++) {
            switch (*pRom++) {
                case ZCR:
                    pViaModeTable->powerOn[i].port[j] = 0xD4;
                    break;
                case ZSR:
                    pViaModeTable->powerOn[i].port[j] = 0xC4;
                    break;
                case ZGR:
                    pViaModeTable->powerOn[i].port[j] = 0xCE;
                    break;
                default:
                    pViaModeTable->powerOn[i].port[j] = *pRom;
                    break;
            }
            /*pViaModeTable->powerOn[i].port[j] = *pRom++;*/
            pViaModeTable->powerOn[i].offset[j] = *pRom++;
            pViaModeTable->powerOn[i].mask[j] = *pRom++;
            pViaModeTable->powerOn[i].data[j] = *pRom++;
            pViaModeTable->powerOn[i].delay[j] = *((CARD16 *)pRom);
            pRom += 2;
        }

        pViaModeTable->powerOn[i].numEntry = j;
    }

    pViaModeTable->NumPowerOn = i;

    /* Get start of PowerOff Seqence Table */
    if (VIAGetTVTabVer(pBIOSInfo, pBIOS) == 3) {
        pRom = pTable + VIA_BIOS_LCDPOWEROFF_POS;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "LCD_POWER_OFF: %X\n", *((CARD16 *)pRom)));
        pRom = pBIOS + *((CARD16 *)pRom);
    }
    else {
        pRom = pTable + VIA_BIOS_LCDPOWEROFF_POS - 4;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "LCD_POWER_OFF: %X\n", *((CARD16 *)pRom)));
        pRom = pBIOS + *((CARD16 *)pRom);
    }

    for (i = 0; (*((CARD16 *)pRom)) != 0xFFFF; i++) {
        if ((*pRom) == 0xFF)
            pRom++;

        pViaModeTable->powerOff[i].powerSeq = *pRom++;

        pRom += 2;

        for (j = 0; *pRom != 0xFF; j++) {
            switch (*pRom++) {
                case ZCR:
                    pViaModeTable->powerOff[i].port[j] = 0xD4;
                    break;
                case ZSR:
                    pViaModeTable->powerOff[i].port[j] = 0xC4;
                    break;
                case ZGR:
                    pViaModeTable->powerOff[i].port[j] = 0xCE;
                    break;
                default:
                    pViaModeTable->powerOff[i].port[j] = *pRom;
                    break;
            }
            /*pViaModeTable->powerOff[i].port[j] = *pRom++;*/
            pViaModeTable->powerOff[i].offset[j] = *pRom++;
            pViaModeTable->powerOff[i].mask[j] = *pRom++;
            pViaModeTable->powerOff[i].data[j] = *pRom++;
            pViaModeTable->powerOff[i].delay[j] = *((CARD16 *)pRom);
            pRom += 2;
        }

        pViaModeTable->powerOff[i].numEntry = j;
    }

    pViaModeTable->NumPowerOff = i;

    /* Get start of Mode Fix Table */
    if (VIAGetTVTabVer(pBIOSInfo, pBIOS) == 3) {
        pRom = pTable + VIA_BIOS_LCDMODEFIX_POS;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "LCD_MODEFIX: %X\n", *((CARD16 *)pRom)));
        pRom = pBIOS + *((CARD16 *)pRom);
    }
    else {
        pRom = pTable + VIA_BIOS_LCDMODEFIX_POS - 4;
        DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "LCD_MODEFIX: %X\n", *((CARD16 *)pRom)));
        pRom = pBIOS + *((CARD16 *)pRom);
    }

    for (i = 0; *pRom != 0xFF; i++) {
        pViaModeTable->modeFix.reqMode[i] = *pRom++;
        pViaModeTable->modeFix.fixMode[i] = *pRom++;
    }

    pViaModeTable->modeFix.numEntry = i;

    xf86Msg(X_DEFAULT, "VIAGetBIOSTable Done\n");

/* Create Mode Table Header File */
#ifdef CREATE_MODETABLE_HEADERFILE
    if (!VIACreateHeaderFile(pViaModeTable)) {
        xfree(pBIOS);
        xfree(pViaModeTable->BIOSDate);
        xfree(pViaModeTable->Modes);
        return FALSE;
    }
#endif /* CREATE_MODETABLE_HEADERFILE */

#ifdef CREATE_TV2_HEADERFILE
    if (VIAGetTVTabVer(pBIOSInfo, pBIOS) != 2) {
        ErrorF("BIOS version is wrong!! There is no TV2+ table.\n");
        xfree(pBIOS);
        xfree(pViaModeTable->BIOSDate);
        xfree(pViaModeTable->Modes);
        return FALSE;
    }
    else {
        if (!VIACreateTV2(pViaModeTable)) {
            xfree(pBIOS);
            xfree(pViaModeTable->BIOSDate);
            xfree(pViaModeTable->Modes);
            return FALSE;
        }
    }
#endif /* CREATE_TV2_HEADERFILE */

#ifdef DBG_MODETABLE_FILE
    VIAPrintModeTableFile(pViaModeTable, pBIOS);
#endif /* DBG_MODETABLE_FILE */

    xfree(pBIOS);
    return TRUE;
}

int VIAFindSupportRefreshRate(VIABIOSInfoPtr pBIOSInfo, int resIndex)
{
    int             bppIndex, refIndex;
    int             needRefresh;
    const int       *supRefTab;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAFindSupportRefreshRate\n"));
    bppIndex = 0;
    supRefTab = NULL;
    needRefresh = pBIOSInfo->Refresh;

    if (needRefresh <= supportRef[0]) {
        refIndex = 0;
    }
    else if (needRefresh >= supportRef[VIA_NUM_REFRESH_RATE - 1]) {
        refIndex = VIA_NUM_REFRESH_RATE - 1;
    }
    else {
        for (refIndex = 0; refIndex < VIA_NUM_REFRESH_RATE; refIndex++) {
            if (needRefresh < supportRef[refIndex + 1]) {
                break;
            }
        }
    }

    switch (pBIOSInfo->bitsPerPixel) {
        case 8:
            bppIndex = 0;
            break;
        case 16:
            bppIndex = 1;
            break;
        case 24:
        case 32:
            bppIndex = 2;
            break;
    }

    switch (pBIOSInfo->MemClk) {
        case VIA_MEM_SDR66:
        case VIA_MEM_SDR100:
            supRefTab = SDR100[bppIndex][resIndex];
            break;
        case VIA_MEM_SDR133:
            supRefTab = SDR133[bppIndex][resIndex];
            break;
        case VIA_MEM_DDR200:
            supRefTab = DDR200[bppIndex][resIndex];
            break;
        case VIA_MEM_DDR266:
        case VIA_MEM_DDR333:
        case VIA_MEM_DDR400:
            supRefTab = DDR266[bppIndex][resIndex];
            break;
    }

    for ( ; refIndex >= 0; refIndex--) {
        if (supRefTab[refIndex]) {
            needRefresh = supportRef[refIndex];
            break;
        }
    }

    pBIOSInfo->FoundRefresh = needRefresh;
    return refIndex;
}


Bool VIAFindModeUseBIOSTable(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    VIAModeTablePtr pViaModeTable;
    int             refresh, maxRefresh, needRefresh, refreshMode;
    int             refIndex;
    int             i, j, k;
    int             modeNum, tmp;
    Bool            setVirtual = FALSE;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAFindModeUseBIOSTable\n"));
    pViaModeTable = pBIOSInfo->pModeTable;

    i = 0;
    j = VIA_RES_INVALID;

    /* detemine support highest resolution by Memory clk */
    VGAOUT8(0x3D4, 0x3D);
    pBIOSInfo->MemClk = (VGAIN8(0x3D5) & 0xF0) >> 4;
    if ((pBIOSInfo->bitsPerPixel > 16) && (pBIOSInfo->HDisplay == 1600) &&
        (pBIOSInfo->VDisplay == 1200) && (pBIOSInfo->MemClk < VIA_MEM_DDR266)) {
        ErrorF("\n1600x1200 True Color only support in MEMCLK higher than DDR266 platform!!\n");
        ErrorF("Please use lower bpp or resolution.\n");
        return FALSE;
    }

    if ((pBIOSInfo->ActiveDevice & VIA_DEVICE_DFP) && (pBIOSInfo->PanelSize == VIA_PANEL_INVALID)) {
        VIAGetPanelInfo(pBIOSInfo);
    }

    pBIOSInfo->UserSetting->DefaultSetting = FALSE;
        
    if (!pBIOSInfo->ActiveDevice) {
        pBIOSInfo->ActiveDevice = VIAGetDeviceDetect(pBIOSInfo);
    }
    /* TV + LCD/DVI has no simultaneous, block it */
    if ((pBIOSInfo->ActiveDevice & VIA_DEVICE_TV)
     && (pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP))) {
        pBIOSInfo->ActiveDevice = VIA_DEVICE_TV;
    }

    if ((pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) &&
        (pBIOSInfo->CrtcHDisplay > 1024) && (
        (pBIOSInfo->TVEncoder == VIA_TV3) ||
        (pBIOSInfo->TVEncoder == VIA_VT1622A) ||
        (pBIOSInfo->TVEncoder == VIA_CH7019) ||
		(pBIOSInfo->TVEncoder == VIA_SAA7108) ||
		(pBIOSInfo->TVEncoder == VIA_FS454))) {
        for (i = 0; i < pViaModeTable->NumModes; i++) {
            if ((pViaModeTable->Modes[i].Bpp == pBIOSInfo->bitsPerPixel) &&
                (pViaModeTable->Modes[i].Width == 1024) &&
                (pViaModeTable->Modes[i].Height == 768))
                break;
        }

        if (i == pViaModeTable->NumModes) {
            ErrorF("\nVIASetModeUseBIOSTable: Cannot find suitable mode!!\n");
            ErrorF("Mode setting in XF86Config(-4) is not supported!!\n");
            return FALSE;
        }

        j = VIA_RES_1024X768;
        pBIOSInfo->frameX1 = 1023;
        pBIOSInfo->frameY1 = 767;
        pBIOSInfo->HDisplay = 1024;
        pBIOSInfo->VDisplay = 768;
        pBIOSInfo->CrtcHDisplay = 1024;
        pBIOSInfo->CrtcVDisplay = 768;
    }
    else if ((pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) && (pBIOSInfo->CrtcHDisplay > 800)
     && (pBIOSInfo->TVEncoder == VIA_TV2PLUS)) {
        for (i = 0; i < pViaModeTable->NumModes; i++) {
            if ((pViaModeTable->Modes[i].Bpp == pBIOSInfo->bitsPerPixel) &&
                (pViaModeTable->Modes[i].Width == 800) &&
                (pViaModeTable->Modes[i].Height == 600))
                break;
        }

        if (i == pViaModeTable->NumModes) {
            ErrorF("\nVIASetModeUseBIOSTable: Cannot find suitable mode!!\n");
            ErrorF("Mode setting in XF86Config(-4) is not supported!!\n");
            return FALSE;
        }

        j = VIA_RES_800X600;
        pBIOSInfo->frameX1 = 799;
        pBIOSInfo->frameY1 = 599;
        pBIOSInfo->HDisplay = 800;
        pBIOSInfo->VDisplay = 600;
        pBIOSInfo->CrtcHDisplay = 800;
        pBIOSInfo->CrtcVDisplay = 600;
    }
    else {
        for (i = 0; i < pViaModeTable->NumModes; i++) {
            if ((pViaModeTable->Modes[i].Bpp == pBIOSInfo->bitsPerPixel) &&
                (pViaModeTable->Modes[i].Width == pBIOSInfo->CrtcHDisplay) &&
                (pViaModeTable->Modes[i].Height == pBIOSInfo->CrtcVDisplay))
                break;
        }

        if (i == pViaModeTable->NumModes) {
            ErrorF("\nVIASetModeUseBIOSTable: Cannot find suitable mode!!\n");
            ErrorF("Mode setting in XF86Config(-4) is not supported!!\n");
            return FALSE;
        }

        modeNum = (int)pViaModeTable->Modes[i].Mode;

    if (pBIOSInfo->ActiveDevice & (VIA_DEVICE_DFP | VIA_DEVICE_LCD)) {
            switch (pBIOSInfo->PanelSize) {
                case VIA_PANEL6X4:
                    pBIOSInfo->panelX = 640;
                    pBIOSInfo->panelY = 480;
                    j = VIA_RES_640X480;
                    break;
                case VIA_PANEL8X6:
                    pBIOSInfo->panelX = 800;
                    pBIOSInfo->panelY = 600;
                    j = VIA_RES_800X600;
                    break;
                case VIA_PANEL10X7:
                    pBIOSInfo->panelX = 1024;
                    pBIOSInfo->panelY = 768;
                    j = VIA_RES_1024X768;
                    break;
                case VIA_PANEL12X7:
                    pBIOSInfo->panelX = 1280;
                    pBIOSInfo->panelY = 768;
                    j = VIA_RES_1280X768;
                    break;
                case VIA_PANEL12X10:
                    pBIOSInfo->panelX = 1280;
                    pBIOSInfo->panelY = 1024;
                    j = VIA_RES_1280X1024;
                    break;
                case VIA_PANEL14X10:
                    pBIOSInfo->panelX = 1400;
                    pBIOSInfo->panelY = 1050;
                    j = VIA_RES_1400X1050;
                    break;
                case VIA_PANEL16X12:
                    pBIOSInfo->panelX = 1600;
                    pBIOSInfo->panelY = 1200;
                    j = VIA_RES_1600X1200;
                    break;
                default:
                    pBIOSInfo->PanelSize = VIA_PANEL10X7;
                    pBIOSInfo->panelX = 1024;
                    pBIOSInfo->panelY = 768;
                    j = VIA_RES_1024X768;
                    break;
            }

            /* Find Panel Size Index */
            for (k = 0; k < VIA_BIOS_NUM_PANEL; k++) {
                if (pViaModeTable->lcdTable[k].fpSize == pBIOSInfo->PanelSize)
                    break;
            };

            tmp = 0x1;
            tmp = tmp << (modeNum & 0xF);
            if ((CARD16)(tmp) &
                pViaModeTable->lcdTable[k].SuptMode[(modeNum >> 4)]) {
            }
            else {
                if ((pBIOSInfo->CrtcHDisplay > pBIOSInfo->panelX) &&
                    (pBIOSInfo->CrtcVDisplay > pBIOSInfo->panelY)) {
                    setVirtual = TRUE;
                    pBIOSInfo->frameX1 = pBIOSInfo->panelX - 1;
                    pBIOSInfo->frameY1 = pBIOSInfo->panelY - 1;
                    pBIOSInfo->HDisplay = pBIOSInfo->panelX;
                    pBIOSInfo->VDisplay = pBIOSInfo->panelY;
                    pBIOSInfo->CrtcHDisplay = pBIOSInfo->panelX;
                    pBIOSInfo->CrtcVDisplay = pBIOSInfo->panelY;
                }
                else {
                    pBIOSInfo->DVIAttach = FALSE;
                    pBIOSInfo->scaleY = FALSE;
                    pBIOSInfo->panelX = 0;
                    pBIOSInfo->panelY = 0;
                }
            }
        }

        if (setVirtual) {
            for (i = 0; i < pViaModeTable->NumModes; i++) {
                if ((pViaModeTable->Modes[i].Bpp == pBIOSInfo->bitsPerPixel) &&
                    (pViaModeTable->Modes[i].Width == pBIOSInfo->HDisplay) &&
                    (pViaModeTable->Modes[i].Height == pBIOSInfo->VDisplay))
                    break;
            }

            if (i == pViaModeTable->NumModes) {
                ErrorF("\nVIASetModeUseBIOSTable: Cannot find suitable mode!!\n");
                ErrorF("Mode setting in XF86Config(-4) is not supported!!\n");
                return FALSE;
            }
        }

        switch (pBIOSInfo->CrtcVDisplay) {
            case 480:
                switch (pBIOSInfo->CrtcHDisplay) {
                    case 640:
                        j = VIA_RES_640X480;
                        break;
                    case 720:
                        j = VIA_RES_720X480;
                        break;
                    case 848:
                        j = VIA_RES_848X480;
                        break;
                    case 856:
                        j = VIA_RES_856X480;
                        break;
                    default:
                        break;
                }
                break;
            case 512:
                j = VIA_RES_1024X512;
                break;
            case 576:
                switch (pBIOSInfo->CrtcHDisplay) {
                    case 720:
                        j = VIA_RES_720X576;
                        break;
                    case 1024:
                        j = VIA_RES_1024X576;
                        break;
                    default:
                        break;
                }
                break;
            case 600:
                j = VIA_RES_800X600;
                break;
            case 768:
                switch (pBIOSInfo->CrtcHDisplay) {
                    case 1024:
                        j = VIA_RES_1024X768;
                        break;
                    case 1280:
                        j = VIA_RES_1280X768;
                        break;
                    default:
                        break;
                }
                break;
            case 864:
                j = VIA_RES_1152X864;
                break;
            case 960:
                j = VIA_RES_1280X960;
                break;
            case 1024:
                j = VIA_RES_1280X1024;
                break;
            case 1050:
                switch (pBIOSInfo->CrtcHDisplay) {
                    case 1440:
                        j = VIA_RES_1440X1050;
                        break;
                    case 1400:
                        j = VIA_RES_1400X1050;
                                break;
                    default:
                        break;
                }
                break;
            case 1200:
                j = VIA_RES_1600X1200;
                break;
            default:
                j = VIA_RES_INVALID;
                break;
        }
    }

    k = 0;

    if (j != VIA_RES_INVALID) {
        if (pBIOSInfo->OptRefresh) {
            pBIOSInfo->Refresh = pBIOSInfo->OptRefresh;
            refIndex = VIAFindSupportRefreshRate(pBIOSInfo, j);
            needRefresh = pBIOSInfo->FoundRefresh;
            if (refIndex < 0) {
                xf86DrvMsg(pBIOSInfo->scrnIndex, X_ERROR, "Mode setting in XF86Config(-4) is not supported!!\n");
                xf86DrvMsg(pBIOSInfo->scrnIndex, X_ERROR, "Please use lower bpp or resolution.\n");
                return FALSE;
            }
        }
        else {
            /* use the monitor information */
            /* needRefresh = (pBIOSInfo->Clock * 1000) / (pBIOSInfo->HTotal * pBIOSInfo->VTotal); */
            /* Do rounding */
            needRefresh = ((pBIOSInfo->Clock * 10000) / (pBIOSInfo->HTotal * pBIOSInfo->VTotal) + 5) / 10;
            pBIOSInfo->Refresh = needRefresh;
            refIndex = VIAFindSupportRefreshRate(pBIOSInfo, j);
            needRefresh = pBIOSInfo->FoundRefresh;
            if (refIndex < 0) {
                xf86DrvMsg(pBIOSInfo->scrnIndex, X_ERROR, "Mode setting in XF86Config(-4) is not supported!!\n");
                xf86DrvMsg(pBIOSInfo->scrnIndex, X_ERROR, "Please use lower bpp or resolution.\n");
                return FALSE;
            }
        }

        refreshMode = 0xFF;
        maxRefresh = 0;

        while (pViaModeTable->refreshTable[j][k].refresh != 0x0) {
            refresh = pViaModeTable->refreshTable[j][k].refresh;
            if (refresh != 0xFF) {
                if ((refresh <= needRefresh) && (refresh > maxRefresh)) {
                    refreshMode = k;
                    maxRefresh = refresh;
                }
            }
            k++;
        }

        if ((refreshMode == 0xFF) && (needRefresh < 60)) {
            xf86DrvMsg(pBIOSInfo->scrnIndex, X_ERROR, "VIASetModeUseBIOSTable: Cannot Find suitable refresh!!\n");
            return FALSE;
        }

        k = refreshMode;
    }

    pBIOSInfo->mode = i;
    pBIOSInfo->resMode = j;
    pBIOSInfo->refresh = k;
    /* pBIOSInfo->widthByQWord = (pBIOSInfo->displayWidth * (pBIOSInfo->bitsPerPixel >> 3)) >> 3; */
    pBIOSInfo->offsetWidthByQWord = (pBIOSInfo->displayWidth * (pBIOSInfo->bitsPerPixel >> 3)) >> 3;
    pBIOSInfo->countWidthByQWord = (pBIOSInfo->CrtcHDisplay * (pBIOSInfo->bitsPerPixel >> 3)) >> 3;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "pBISOInfo->FoundRefresh: %d\n", pBIOSInfo->FoundRefresh));
    return TRUE;
}

void VIASetLCDMode(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           modeNum, tmp;
    int             resIdx;
    int             port, offset, data;
    int             resMode = pBIOSInfo->resMode;
    int             i, j, k, misc;


    modeNum = (CARD8)(pViaModeTable->Modes[pBIOSInfo->mode].Mode);

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASetLCDMode\n"));
    /* Panel Size 1600x1200 Not Supported Now*/
    if (pBIOSInfo->PanelSize == VIA_PANEL16X12) {
        xfree(pViaModeTable->BIOSDate);
        xfree(pViaModeTable->Modes);
        xf86DrvMsg(pBIOSInfo->scrnIndex, X_ERROR, "VIASetModeUseBIOSTable: Panel Size Not Support!!\n");
    }

    /* Find Panel Size Index */
    for (i = 0; i < VIA_BIOS_NUM_PANEL; i++) {
        if (pViaModeTable->lcdTable[i].fpSize == pBIOSInfo->PanelSize)
            break;
    };

    if (i == VIA_BIOS_NUM_PANEL) {
        xfree(pViaModeTable->BIOSDate);
        xfree(pViaModeTable->Modes);
        xf86DrvMsg(pBIOSInfo->scrnIndex, X_ERROR, "VIASetModeUseBIOSTable: Panel Size Not Support!!\n");
    }

    if (pBIOSInfo->PanelSize == VIA_PANEL12X10) {
        VGAOUT8(0x3d4, 0x89);
        VGAOUT8(0x3d5, 0x07);
    }

    /* LCD Expand Mode Y Scale Flag */
    pBIOSInfo->scaleY = FALSE;

    /* Set LCD InitTb Regs */

    /* Set LClk */
    VGAOUT8(0x3d4, 0x17);
    tmp = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, tmp & 0x7F);

    if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
        VGAOUT8(0x3c4, 0x44);
        VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk_12Bit >> 8));
        VGAOUT8(0x3c4, 0x45);
        VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk_12Bit & 0xFF));
    }
    else {
        VGAOUT8(0x3c4, 0x44);
        VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk >> 8));
        VGAOUT8(0x3c4, 0x45);
        VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk & 0xFF));
    }
    VGAOUT8(0x3d4, 0x17);
    tmp = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, tmp | 0x80);

    VGAOUT8(0x3c4, 0x40);
    tmp = VGAIN8(0x3c5);
    VGAOUT8(0x3c5, tmp | 0x04);
    VGAOUT8(0x3c4, 0x40);
    tmp = VGAIN8(0x3c5);
    VGAOUT8(0x3c5, tmp & 0xFB);

    if (!pBIOSInfo->IsSecondary) {
        /* Set VClk */
        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp & 0x7F);

        if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
            VGAOUT8(0x3c4, 0x46);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk_12Bit >> 8));
            VGAOUT8(0x3c4, 0x47);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk_12Bit & 0xFF));
        }
        else {
            VGAOUT8(0x3c4, 0x46);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk >> 8));
            VGAOUT8(0x3c4, 0x47);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk & 0xFF));
        }
        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x80);

        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp | 0x02);
        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp & 0xFD);
    }

    /* Use external clock */
    data = VGAIN8(0x3cc) | 0x0C;
    VGAOUT8(0x3c2, data);

    for (j = 0;
         j < pViaModeTable->lcdTable[i].InitTb.numEntry;
         j++)
    {
        port = pViaModeTable->lcdTable[i].InitTb.port[j];
        offset = pViaModeTable->lcdTable[i].InitTb.offset[j];
        data = pViaModeTable->lcdTable[i].InitTb.data[j];
        VGAOUT8(0x300+port, offset);
        VGAOUT8(0x301+port, data);
    }

    if ((pBIOSInfo->CrtcHDisplay == pBIOSInfo->panelX) &&
        (pBIOSInfo->CrtcVDisplay == pBIOSInfo->panelY)) {
        /* Set LClk */
        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp & 0x7F);

        if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
            VGAOUT8(0x3c4, 0x44);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk_12Bit >> 8));
            VGAOUT8(0x3c4, 0x45);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk_12Bit & 0xFF));
        }
        else {
            VGAOUT8(0x3c4, 0x44);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk >> 8));
            VGAOUT8(0x3c4, 0x45);
            VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.LCDClk & 0xFF));
        }

        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x80);

        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp | 0x04);
        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp & 0xFB);

        if (!pBIOSInfo->IsSecondary) {
            /* Set VClk */
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                VGAOUT8(0x3c4, 0x46);
                VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk_12Bit >> 8));
                VGAOUT8(0x3c4, 0x47);
                VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk_12Bit & 0xFF));
            }
            else {
                VGAOUT8(0x3c4, 0x46);
                VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk >> 8));
                VGAOUT8(0x3c4, 0x47);
                VGAOUT8(0x3c5, (pViaModeTable->lcdTable[i].InitTb.VClk & 0xFF));
            }

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x02);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFD);
        }

        /* Use external clock */
        data = VGAIN8(0x3cc) | 0x0C;
        VGAOUT8(0x3c2, data);
    }
    else {
        if (!(pBIOSInfo->Center)) {
            /* LCD Expand Mode Y Scale Flag */
            pBIOSInfo->scaleY = TRUE;
        }

        resIdx = VIA_RES_INVALID;

        /* Find MxxxCtr & MxxxExp Index and
         * HWCursor Y Scale (PanelSize Y / Res. Y) */
        pBIOSInfo->resY = pBIOSInfo->CrtcVDisplay;
        switch (resMode) {
            case VIA_RES_640X480:
                resIdx = 0;
                break;
            case VIA_RES_800X600:
                resIdx = 1;
                break;
            case VIA_RES_1024X768:
                resIdx = 2;
                break;
            case VIA_RES_1152X864:
                resIdx = 3;
                break;
            case VIA_RES_1280X768:
            case VIA_RES_1280X960:
            case VIA_RES_1280X1024:
                if (pBIOSInfo->PanelSize == VIA_PANEL12X10)
                    resIdx = VIA_RES_INVALID;
                else
                    resIdx = 4;
                break;
            default:
                resIdx = VIA_RES_INVALID;
                break;
        }

        if ((pBIOSInfo->CrtcHDisplay == 640) &&
            (pBIOSInfo->CrtcVDisplay == 400))
            resIdx = 0;

        if (pBIOSInfo->Center) {
            if (resIdx != VIA_RES_INVALID) {
            /* Set LCD MxxxCtr Regs */
            for (j = 0;
                 j < pViaModeTable->lcdTable[i].MCtr[resIdx].numEntry;
                 j++)
            {
                port = pViaModeTable->lcdTable[i].MCtr[resIdx].port[j];
                offset = pViaModeTable->lcdTable[i].MCtr[resIdx].offset[j];
                data = pViaModeTable->lcdTable[i].MCtr[resIdx].data[j];
                VGAOUT8(0x300+port, offset);
                VGAOUT8(0x301+port, data);
            }

            /* Set LClk */
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                VGAOUT8(0x3c4, 0x44);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MCtr[resIdx].LCDClk_12Bit >> 8));
                VGAOUT8(0x3c4, 0x45);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MCtr[resIdx].LCDClk_12Bit & 0xFF));
            }
            else {
                VGAOUT8(0x3c4, 0x44);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MCtr[resIdx].LCDClk >> 8));
                VGAOUT8(0x3c4, 0x45);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MCtr[resIdx].LCDClk & 0xFF));
            }

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x04);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFB);

            if (!pBIOSInfo->IsSecondary) {
                /* Set VClk */
                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp & 0x7F);

                if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                    VGAOUT8(0x3c4, 0x46);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MCtr[resIdx].VClk_12Bit >> 8));
                    VGAOUT8(0x3c4, 0x47);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MCtr[resIdx].VClk_12Bit & 0xFF));
                }
                else {
                    VGAOUT8(0x3c4, 0x46);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MCtr[resIdx].VClk >> 8));
                    VGAOUT8(0x3c4, 0x47);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MCtr[resIdx].VClk & 0xFF));
                }

                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp | 0x80);

                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp | 0x02);
                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp & 0xFD);
            }
            }

            for (j = 0; j < pViaModeTable->modeFix.numEntry; j++) {
                if (pViaModeTable->modeFix.reqMode[j] == modeNum) {
                    modeNum = pViaModeTable->modeFix.fixMode[j];
                    break;
                }
            }

            for (j = 0; j < pViaModeTable->lcdTable[i].numMPatchDP2Ctr; j++) {
                if (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].Mode == modeNum)
                    break;
            }

            if (j != pViaModeTable->lcdTable[i].numMPatchDP2Ctr) {
                /* Set LCD MPatchDP2Ctr Regs */
                for (k = 0;
                     k < pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].numEntry;
                     k++)
                {
                    port = pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].port[k];
                    offset = pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].offset[k];
                    data = pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].data[k];
                VGAOUT8(0x300+port, offset);
                VGAOUT8(0x301+port, data);
            }

            /* Set LClk */
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                VGAOUT8(0x3c4, 0x44);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].LCDClk_12Bit >> 8));
                VGAOUT8(0x3c4, 0x45);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].LCDClk_12Bit & 0xFF));
            }
            else {
                VGAOUT8(0x3c4, 0x44);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].LCDClk >> 8));
                VGAOUT8(0x3c4, 0x45);
                VGAOUT8(0x3c5,
                        (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].LCDClk & 0xFF));
            }

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x04);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFB);

            if (!pBIOSInfo->IsSecondary) {
                /* Set VClk */
                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp & 0x7F);

                if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                    VGAOUT8(0x3c4, 0x46);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].VClk_12Bit >> 8));
                    VGAOUT8(0x3c4, 0x47);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].VClk_12Bit & 0xFF));
                }
                else {
                    VGAOUT8(0x3c4, 0x46);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].VClk >> 8));
                    VGAOUT8(0x3c4, 0x47);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Ctr[j].VClk & 0xFF));
                }

                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp | 0x80);

                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp | 0x02);
                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp & 0xFD);
            }
            }

            for (j = 0; j < pViaModeTable->lcdTable[i].numMPatchDP1Ctr; j++) {
                if (pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].Mode == modeNum)
                    break;
            }

            if ((j != pViaModeTable->lcdTable[i].numMPatchDP1Ctr) &&
                pBIOSInfo->IsSecondary) {
                /* Set LCD MPatchDP1Ctr Regs */
                for (k = 0;
                     k < pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].numEntry;
                     k++)
                {
                    port = pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].port[k];
                    offset = pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].offset[k];
                    data = pViaModeTable->lcdTable[i].MPatchDP1Ctr[j].data[k];
                    VGAOUT8(0x300+port, offset);
                    VGAOUT8(0x301+port, data);
                }
            }

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);

        }
        else {
            if (resIdx != VIA_RES_INVALID) {
                /* Set LCD MxxxExp Regs */
                for (j = 0;
                     j < pViaModeTable->lcdTable[i].MExp[resIdx].numEntry;
                     j++)
                {
                    port = pViaModeTable->lcdTable[i].MExp[resIdx].port[j];
                    offset = pViaModeTable->lcdTable[i].MExp[resIdx].offset[j];
                    data = pViaModeTable->lcdTable[i].MExp[resIdx].data[j];
                    VGAOUT8(0x300+port, offset);
                    VGAOUT8(0x301+port, data);
                }

                /* Set LClk */
                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp & 0x7F);

                if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                    VGAOUT8(0x3c4, 0x44);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MExp[resIdx].LCDClk_12Bit >> 8));
                    VGAOUT8(0x3c4, 0x45);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MExp[resIdx].LCDClk_12Bit & 0xFF));
                }
                else {
                    VGAOUT8(0x3c4, 0x44);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MExp[resIdx].LCDClk >> 8));
                    VGAOUT8(0x3c4, 0x45);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MExp[resIdx].LCDClk & 0xFF));
                }

                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp | 0x80);

                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp | 0x04);
                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp & 0xFB);

                if (!pBIOSInfo->IsSecondary) {
                    /* Set VClk */
                    VGAOUT8(0x3d4, 0x17);
		    tmp = VGAIN8(0x3d5);
                    VGAOUT8(0x3d5, tmp & 0x7F);

                    if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                        VGAOUT8(0x3c4, 0x46);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MExp[resIdx].VClk_12Bit >> 8));
                        VGAOUT8(0x3c4, 0x47);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MExp[resIdx].VClk_12Bit & 0xFF));
                    }
                    else {
                        VGAOUT8(0x3c4, 0x46);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MExp[resIdx].VClk >> 8));
                        VGAOUT8(0x3c4, 0x47);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MExp[resIdx].VClk & 0xFF));
                    }

                    VGAOUT8(0x3d4, 0x17);
		    tmp = VGAIN8(0x3d5);
                    VGAOUT8(0x3d5, tmp | 0x80);

                    VGAOUT8(0x3c4, 0x40);
		    tmp = VGAIN8(0x3c5);
                    VGAOUT8(0x3c5, tmp | 0x02);
                    VGAOUT8(0x3c4, 0x40);
		    tmp = VGAIN8(0x3c5);
                    VGAOUT8(0x3c5, tmp & 0xFD);
                }
            }

            for (j = 0; j < pViaModeTable->modeFix.numEntry; j++) {
                if (pViaModeTable->modeFix.reqMode[j] == modeNum) {
                    modeNum = pViaModeTable->modeFix.fixMode[j];
                    break;
                }
            }

            for (j = 0; j < pViaModeTable->lcdTable[i].numMPatchDP2Exp; j++) {
                if (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].Mode == modeNum)
                    break;
            }

            if (j != pViaModeTable->lcdTable[i].numMPatchDP2Exp) {
                if (pBIOSInfo->CrtcHDisplay == pBIOSInfo->panelX)
                    pBIOSInfo->scaleY = FALSE;
                /* Set LCD MPatchExp Regs */
                for (k = 0;
                     k < pViaModeTable->lcdTable[i].MPatchDP2Exp[j].numEntry;
                     k++)
                {
                    port = pViaModeTable->lcdTable[i].MPatchDP2Exp[j].port[k];
                    offset = pViaModeTable->lcdTable[i].MPatchDP2Exp[j].offset[k];
                    data = pViaModeTable->lcdTable[i].MPatchDP2Exp[j].data[k];
                    VGAOUT8(0x300+port, offset);
                    VGAOUT8(0x301+port, data);
                }

                /* Set LClk */
                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp & 0x7F);

                if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                    VGAOUT8(0x3c4, 0x44);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].LCDClk_12Bit >> 8));
                    VGAOUT8(0x3c4, 0x45);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].LCDClk_12Bit & 0xFF));
                }
                else {
                    VGAOUT8(0x3c4, 0x44);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].LCDClk >> 8));
                    VGAOUT8(0x3c4, 0x45);
                    VGAOUT8(0x3c5,
                            (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].LCDClk & 0xFF));
                }

                VGAOUT8(0x3d4, 0x17);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp | 0x80);

                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp | 0x04);
                VGAOUT8(0x3c4, 0x40);
		tmp = VGAIN8(0x3c5);
                VGAOUT8(0x3c5, tmp & 0xFB);

                if (!pBIOSInfo->IsSecondary) {
                    /* Set VClk */
                    VGAOUT8(0x3d4, 0x17);
		    tmp = VGAIN8(0x3d5);
                    VGAOUT8(0x3d5, tmp & 0x7F);

                    if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                        VGAOUT8(0x3c4, 0x46);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].VClk_12Bit >> 8));
                        VGAOUT8(0x3c4, 0x47);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].VClk_12Bit & 0xFF));
                    }
                    else {
                        VGAOUT8(0x3c4, 0x46);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].VClk >> 8));
                        VGAOUT8(0x3c4, 0x47);
                        VGAOUT8(0x3c5,
                                (pViaModeTable->lcdTable[i].MPatchDP2Exp[j].VClk & 0xFF));
                    }

                    VGAOUT8(0x3d4, 0x17);
		    tmp = VGAIN8(0x3d5);
                    VGAOUT8(0x3d5, tmp | 0x80);

                    VGAOUT8(0x3c4, 0x40);
		    tmp = VGAIN8(0x3c5);
                    VGAOUT8(0x3c5, tmp | 0x02);
                    VGAOUT8(0x3c4, 0x40);
		    tmp = VGAIN8(0x3c5);
                    VGAOUT8(0x3c5, tmp & 0xFD);
                }
            }

            for (j = 0; j < pViaModeTable->lcdTable[i].numMPatchDP1Exp; j++) {
                if (pViaModeTable->lcdTable[i].MPatchDP1Exp[j].Mode == modeNum)
                    break;
            }

            if ((j != pViaModeTable->lcdTable[i].numMPatchDP1Exp) &&
                pBIOSInfo->IsSecondary) {
                /* Set LCD MPatchDP1Ctr Regs */
                for (k = 0;
                     k < pViaModeTable->lcdTable[i].MPatchDP1Exp[j].numEntry;
                     k++)
                {
                    port = pViaModeTable->lcdTable[i].MPatchDP1Exp[j].port[k];
                    offset = pViaModeTable->lcdTable[i].MPatchDP1Exp[j].offset[k];
                    data = pViaModeTable->lcdTable[i].MPatchDP1Exp[j].data[k];
                    VGAOUT8(0x300+port, offset);
                    VGAOUT8(0x301+port, data);
                }
            }

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }
    }

    /* LCD patch 3D5.02 */
    VGAOUT8(0x3d4, 0x01);
    misc = VGAIN8(0x3d5);
    VGAOUT8(0x3d4, 0x02);
    VGAOUT8(0x3d5, misc);

    /* Enable LCD */
    if (!pBIOSInfo->IsSecondary) {
        /* CRT Display Source Bit 6 - 0: CRT, 1: LCD */
        VGAOUT8(0x3c4, 0x16);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp | 0x40);

        /* Enable Simultaneous */
        if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
            VGAOUT8(0x3d4, 0x6B);
            VGAOUT8(0x3d5, 0xA8);
            if (pBIOSInfo->Chipset == VIA_CLE266 && pBIOSInfo->ChipRev < 15) {
                VGAOUT8(0x3d4, 0x93);
                VGAOUT8(0x3d5, 0xB1);
            }
            else {
                VGAOUT8(0x3d4, 0x93);
                VGAOUT8(0x3d5, 0xAF);
            }
        }
        else {
            VGAOUT8(0x3d4, 0x6B);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x08);
            VGAOUT8(0x3d4, 0x93);
            VGAOUT8(0x3d5, 0x0);
        }
        VGAOUT8(0x3d4, 0x6A);
        VGAOUT8(0x3d5, 0x48);
    }
    else {
        /* CRT Display Source Bit 6 - 0: CRT, 1: LCD */
        VGAOUT8(0x3c4, 0x16);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp & ~0x40);

        /* Enable SAMM */
        if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
            VGAOUT8(0x3d4, 0x6B);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x20);
            if (pBIOSInfo->Chipset == VIA_CLE266 && pBIOSInfo->ChipRev < 15) {
                VGAOUT8(0x3d4, 0x93);
                VGAOUT8(0x3d5, 0xB1);
            }
            else {
                VGAOUT8(0x3d4, 0x93);
                VGAOUT8(0x3d5, 0xAF);
            }
        }
        else {
            VGAOUT8(0x3d4, 0x6B);
            VGAOUT8(0x3d5, 0);
            VGAOUT8(0x3d4, 0x93);
            VGAOUT8(0x3d5, 0x0);
        }
        VGAOUT8(0x3d4, 0x6A);
        VGAOUT8(0x3d5, 0xC8);
    }
}

void VIAPreSetTV2Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           *TV;
    CARD16          *DotCrawl, *Patch2;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j;
    unsigned char   W_Buffer[VIA_BIOS_MAX_NUM_TV_REG+1];
    unsigned char   W_Other[2];
    int             w_bytes;
    I2CDevPtr       dev;
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPreSetTV2Mode\n"));
    TV = NULL;
    DotCrawl = NULL;
    Patch2 = NULL;
    W_Buffer[0] = 0;
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        W_Buffer[i+1] = pBIOSInfo->TVRegs[i];
    }

    if (pBIOSInfo->TVType == TVTYPE_PAL) {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                Patch2 = pViaModeTable->tv2Table[tvIndx].PatchPAL2;
                if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
                    TV = pViaModeTable->tv2Table[tvIndx].TVPALC;
                else
                    TV = pViaModeTable->tv2Table[tvIndx].TVPALS;
                break;
            case VIA_TVOVER:
                Patch2 = pViaModeTable->tv2OverTable[tvIndx].PatchPAL2;
                if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
                    TV = pViaModeTable->tv2OverTable[tvIndx].TVPALC;
                else
                    TV = pViaModeTable->tv2OverTable[tvIndx].TVPALS;
                break;
        }
    }
    else {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                Patch2 = pViaModeTable->tv2Table[tvIndx].PatchNTSC2;
                DotCrawl = pViaModeTable->tv2Table[tvIndx].DotCrawlNTSC;
                if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
                    TV = pViaModeTable->tv2Table[tvIndx].TVNTSCC;
                else
                    TV = pViaModeTable->tv2Table[tvIndx].TVNTSCS;
                break;
            case VIA_TVOVER:
                Patch2 = pViaModeTable->tv2OverTable[tvIndx].PatchNTSC2;
                DotCrawl = pViaModeTable->tv2OverTable[tvIndx].DotCrawlNTSC;
                if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
                    TV = pViaModeTable->tv2OverTable[tvIndx].TVNTSCC;
                else
                    TV = pViaModeTable->tv2OverTable[tvIndx].TVNTSCS;
                break;
        }
    }

    /* Set TV mode */
    for (i = 0, j = 0; i < pViaModeTable->tv2MaskTable.numTV
	     && j < VIA_BIOS_MAX_NUM_TV_REG ; j++) {
        if (pViaModeTable->tv2MaskTable.TV[j] == 0xFF) {
            W_Buffer[j+1] = TV[j];
            i++;
        }
    }

    w_bytes = j + 1;

    dev = xf86CreateI2CDevRec();
    dev->DevName = "VT1621";
    dev->SlaveAddr = 0x40;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    xf86I2CDevInit(dev);

    xf86I2CWriteRead(dev, W_Buffer,w_bytes, NULL,0);

    /* Turn on all Composite and S-Video output */
    W_Other[0] = 0x0E;
    W_Other[1] = 0;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);

    if (pBIOSInfo->TVDotCrawl && (pBIOSInfo->TVType == TVTYPE_NTSC)) {
        int numReg = (int)(DotCrawl[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(DotCrawl[i] & 0xFF);
            if (W_Other[0] == 0x11) {
                xf86I2CWriteRead(dev, W_Other,1, R_Buffer,1);
                W_Other[1] = R_Buffer[0] | (unsigned char)(DotCrawl[i] >> 8);
            }
            else {
                W_Other[1] = (unsigned char)(DotCrawl[i] >> 8);
            }
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
    }

    if (pBIOSInfo->IsSecondary) {
        int numPatch;

        /* Patch as setting 2nd path */
        numPatch = (int)(pViaModeTable->tv2MaskTable.misc2 >> 5);
        for (i = 0; i < numPatch; i++) {
            W_Other[0] = (unsigned char)(Patch2[i] & 0xFF);
            W_Other[1] = (unsigned char)(Patch2[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
    }

    xf86DestroyI2CDevRec(dev,TRUE);
}

void VIAPreSetCH7019Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           *TV;
    CARD16          *DotCrawl, *Patch2;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j;
    unsigned char   W_Buffer[VIA_BIOS_MAX_NUM_TV_REG + 1];
    unsigned char   W_Other[2];
    int             w_bytes;
    I2CDevPtr       dev;
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPreSetCH7019Mode\n"));
    DotCrawl = NULL;
    Patch2 = NULL;
    TV = NULL;
    W_Buffer[0] = 0;
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        W_Buffer[i+1] = pBIOSInfo->TVRegs[i];
    }

    if (pBIOSInfo->TVType == TVTYPE_PAL) {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                TV = pViaModeTable->ch7019Table[tvIndx].TVPAL;
                Patch2 = pViaModeTable->ch7019Table[tvIndx].PatchPAL2;
                break;
            case VIA_TVOVER:
                TV = pViaModeTable->ch7019OverTable[tvIndx].TVPAL;
                Patch2 = pViaModeTable->ch7019OverTable[tvIndx].PatchPAL2;
                break;
        }
    }
    else {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                TV = pViaModeTable->ch7019Table[tvIndx].TVNTSC;
                DotCrawl = pViaModeTable->ch7019Table[tvIndx].DotCrawlNTSC;
                Patch2 = pViaModeTable->ch7019Table[tvIndx].PatchNTSC2;
                break;
            case VIA_TVOVER:
                TV = pViaModeTable->ch7019OverTable[tvIndx].TVNTSC;
                DotCrawl = pViaModeTable->ch7019OverTable[tvIndx].DotCrawlNTSC;
                Patch2 = pViaModeTable->ch7019OverTable[tvIndx].PatchNTSC2;
                break;
        }
    }

    for (i = 0, j = 0; i < pViaModeTable->ch7019MaskTable.numTV
	     && j < VIA_BIOS_MAX_NUM_TV_REG ; j++) {
        if (pViaModeTable->ch7019MaskTable.TV[j] == 0xFF) {
            W_Buffer[j+1] = TV[j];
            i++;
        }
    }

    w_bytes = j + 1;

    dev = xf86CreateI2CDevRec();
    dev->DevName = "CH7019";
    dev->SlaveAddr = 0xEA;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    xf86I2CDevInit(dev);

    /* Disable TV avoid set mode garbage */
    W_Other[0] = 0x49;
    W_Other[1] = 0X3E;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0x1E;
    W_Other[1] = 0xD0;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);

    xf86I2CWriteRead(dev, W_Buffer,w_bytes, NULL,0);

    if (pBIOSInfo->TVDotCrawl && (pBIOSInfo->TVType == TVTYPE_NTSC)) {
        int numReg = (int)(DotCrawl[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(DotCrawl[i] & 0xFF);
            if (W_Other[0] == 0x11) {
                xf86I2CWriteRead(dev, W_Other,1, R_Buffer,1);
                W_Other[1] = R_Buffer[0] | (unsigned char)(DotCrawl[i] >> 8);
            }
            else {
                W_Other[1] = (unsigned char)(DotCrawl[i] >> 8);
            }
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
    }

    /* Turn TV CH7019 DAC On */
    W_Other[0] = 0x49;
    W_Other[1] = 0x20;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);

    if (pBIOSInfo->IsSecondary) {
        int numPatch;

        /* Patch as setting 2nd path */
        numPatch = (int)(pViaModeTable->ch7019MaskTable.misc2 >> 5);
        for (i = 0; i < numPatch; i++) {
            W_Other[0] = (unsigned char)(Patch2[i] & 0xFF);
            W_Other[1] = (unsigned char)(Patch2[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
    }

    xf86DestroyI2CDevRec(dev,TRUE);
}

void VIAPreSetFS454Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD16          *TV, *DotCrawl, *RGB, *YCbCr;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             tvType = pBIOSInfo->TVType;
    int             i, numReg;
    unsigned char   W_Buffer[2];
    unsigned char   R_Buffer[1];
    I2CDevPtr       dev;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPreSetFS454Mode\n"));
    TV = NULL;
    DotCrawl = NULL;
    RGB = NULL;
    YCbCr = NULL;
    W_Buffer[0] = 0;

    switch (pBIOSInfo->TVVScan) {
        case VIA_TVNORMAL:
            TV = pViaModeTable->fs454Table[tvIndx].TVNTSC;
            DotCrawl = pViaModeTable->fs454Table[tvIndx].DotCrawlNTSC;
            RGB = pViaModeTable->fs454Table[tvIndx].RGBNTSC;
            YCbCr = pViaModeTable->fs454Table[tvIndx].YCbCrNTSC;
            break;
        case VIA_TVOVER:
            TV = pViaModeTable->fs454OverTable[tvIndx].TVNTSC;
            DotCrawl = pViaModeTable->fs454OverTable[tvIndx].DotCrawlNTSC;
            RGB = pViaModeTable->fs454OverTable[tvIndx].RGBNTSC;
            YCbCr = pViaModeTable->fs454OverTable[tvIndx].YCbCrNTSC;
            break;
    }

    dev = xf86CreateI2CDevRec();
    dev->DevName = "FS454";
    dev->SlaveAddr = 0xD4;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    xf86I2CDevInit(dev);

    /* Turn on all Composite and S-Video output */
    numReg = (int)(TV[0]);
	for (i = 1; i < (numReg + 1); i++){
	    W_Buffer[0] = (unsigned char)(TV[i] & 0xFF);;
	    W_Buffer[1] = (unsigned char)(TV[i] >> 8);
	    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
	}

    if (pBIOSInfo->TVDotCrawl && (tvType == TVTYPE_NTSC)) {
        numReg = (int)(DotCrawl[0]);
        for (i = 1; i < (numReg + 1); i++) {
            W_Buffer[0] = (unsigned char)(DotCrawl[i] & 0xFF);
            if (W_Buffer[0] == 0x11) {
                xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                W_Buffer[1] = R_Buffer[0] | (unsigned char)(DotCrawl[i] >> 8);
            }
            else {
                W_Buffer[1] = (unsigned char)(DotCrawl[i] >> 8);
            }
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        }
    }

    if (pBIOSInfo->TVOutput == TVOUTPUT_RGB) {
        numReg = (int)(RGB[0]);
        for (i = 1; i < (numReg + 1); i++) {
            W_Buffer[0] = (unsigned char)(RGB[i] & 0xFF);
            W_Buffer[1] = (unsigned char)(RGB[i] >> 8);
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        }
    }

    if (pBIOSInfo->TVOutput == TVOUTPUT_YCBCR) {
        numReg = (int)(YCbCr[0]);
        for (i = 1; i < (numReg + 1); i++) {
            W_Buffer[0] = (unsigned char)(YCbCr[i] & 0xFF);
            W_Buffer[1] = (unsigned char)(YCbCr[i] >> 8);
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
        }
    }

    xf86DestroyI2CDevRec(dev,TRUE);
}

void VIAPreSetSAA7108Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           *TV;
    CARD16          *RGB, *YCbCr, *Patch2;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j;
    unsigned char   W_Buffer[VIA_BIOS_MAX_NUM_SAA7108_TV_REG + 1];
    unsigned char   W_Other[4];
    int             w_bytes;
    I2CDevPtr       dev;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPreSetSAA7108Mode\n"));
    RGB = NULL;
    YCbCr = NULL;
    Patch2 = NULL;
    TV = NULL;
    W_Buffer[0] = 0;
    for (i = 0; i < VIA_BIOS_MAX_NUM_SAA7108_TV_REG; i++) {
        W_Buffer[i+1] = pBIOSInfo->TVRegs[i];
    }

    if (pBIOSInfo->TVType == TVTYPE_PAL) {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                TV = pViaModeTable->saa7108Table[tvIndx].TVPAL;
                RGB = pViaModeTable->saa7108Table[tvIndx].RGBPAL;
                YCbCr = pViaModeTable->saa7108Table[tvIndx].YCbCrPAL;
                Patch2 = pViaModeTable->saa7108Table[tvIndx].PatchPAL2;
                break;
            case VIA_TVOVER:
                TV = pViaModeTable->saa7108OverTable[tvIndx].TVPAL;
                RGB = pViaModeTable->saa7108OverTable[tvIndx].RGBPAL;
                YCbCr = pViaModeTable->saa7108OverTable[tvIndx].YCbCrPAL;
                Patch2 = pViaModeTable->saa7108OverTable[tvIndx].PatchPAL2;
                break;
        }
    }
    else {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                TV = pViaModeTable->saa7108Table[tvIndx].TVNTSC;
                RGB = pViaModeTable->saa7108Table[tvIndx].RGBNTSC;
                YCbCr = pViaModeTable->saa7108Table[tvIndx].YCbCrNTSC;
                Patch2 = pViaModeTable->saa7108Table[tvIndx].PatchNTSC2;
                break;
            case VIA_TVOVER:
                TV = pViaModeTable->saa7108OverTable[tvIndx].TVNTSC;
                RGB = pViaModeTable->saa7108OverTable[tvIndx].RGBNTSC;
                YCbCr = pViaModeTable->saa7108OverTable[tvIndx].YCbCrNTSC;
                Patch2 = pViaModeTable->saa7108OverTable[tvIndx].PatchNTSC2;
                break;
        }
    }

    for (i = 0, j = 0; i < pViaModeTable->saa7108MaskTable.numTV
	     && j < VIA_BIOS_MAX_NUM_SAA7108_TV_REG; j++) {
        if (pViaModeTable->saa7108MaskTable.TV[j] == 0xFF) {
            W_Buffer[j+1] = TV[j];
            i++;
        }
    }

    w_bytes = j + 1;

    dev = xf86CreateI2CDevRec();
    dev->DevName = "SAA7108";
    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    xf86I2CDevInit(dev);

    /* Initial SAA7108AE TV Encoder */
    W_Other[0] = 0x2D;
    W_Other[1] = 0x08;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0xFD;
    W_Other[1] = 0x80;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0x37;
    W_Other[1] = 0x12;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0x3A;
    W_Other[1] = 0x04;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0xA2;
    W_Other[1] = 0x0;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0xFA;
    W_Other[1] = 0x07;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0x17;
    W_Other[1] = 0x1B;
    W_Other[2] = 0x1B;
    W_Other[3] = 0x1F;
    xf86I2CWriteRead(dev, W_Other,4, NULL,0);
    W_Other[0] = 0x38;
    W_Other[1] = 0x1A;
    W_Other[2] = 0x1A;
    xf86I2CWriteRead(dev, W_Other,3, NULL,0);
    W_Other[0] = 0x20;
    W_Other[1] = 0x0;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);

    xf86I2CWriteRead(dev, W_Buffer,w_bytes, NULL,0);

    /* Turn on all Composite and S-Video output ,Enable TV */
    if ((pBIOSInfo->TVOutput == TVOUTPUT_SVIDEO) ||
        (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)) {
        W_Other[0] = 0x2D;
        W_Other[1] = 0xB4;
        xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    }
    else if (pBIOSInfo->TVOutput == TVOUTPUT_RGB) {
        int numReg = (int)(RGB[0]);
        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(RGB[i] & 0xFF);
            W_Other[1] = (unsigned char)(RGB[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
        W_Other[0] = 0x2D;
        W_Other[1] = 0;
        xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    }
    else if (pBIOSInfo->TVOutput == TVOUTPUT_YCBCR) {
        int numReg = (int)(YCbCr[0]);
        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(YCbCr[i] & 0xFF);
            W_Other[1] = (unsigned char)(YCbCr[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
        W_Other[0] = 0x2D;
        W_Other[1] = 0x84;
        xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    }

    if (pBIOSInfo->IsSecondary) {
        int numPatch;

        /* Patch as setting 2nd path */
        numPatch = (int)(pViaModeTable->saa7108MaskTable.misc2 >> 5);
        for (i = 0; i < numPatch; i++) {
            W_Other[0] = (unsigned char)(Patch2[i] & 0xFF);
            W_Other[1] = (unsigned char)(Patch2[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
    }

    xf86DestroyI2CDevRec(dev,TRUE);
}

void VIAPreSetVT1623Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    VIABIOSTVMASKTablePtr TVMaskTbl;
    CARD8           *TV;
    CARD16          *DotCrawl, *RGB, *YCbCr, *Patch2;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             tvType = pBIOSInfo->TVType;
    int             i, j;
    unsigned char   W_Buffer[VIA_BIOS_MAX_NUM_TV_REG + 1];
    unsigned char   W_Other[2];
    int             w_bytes;
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPreSetVT1623Mode\n"));
    DotCrawl = NULL;
    RGB = NULL;
    YCbCr = NULL;
    Patch2 = NULL;
    TV = NULL;
    W_Buffer[0] = 0;
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        W_Buffer[i+1] = pBIOSInfo->TVRegs[i];
    }

	TVMaskTbl = &pViaModeTable->vt1622aMaskTable;
    if (tvType == TVTYPE_PAL) {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                TV = pViaModeTable->vt1622aTable[tvIndx].TVPAL;
                RGB = pViaModeTable->vt1622aTable[tvIndx].RGBPAL;
                YCbCr = pViaModeTable->vt1622aTable[tvIndx].YCbCrPAL;
                Patch2 = pViaModeTable->vt1622aTable[tvIndx].PatchPAL2;
                break;
            case VIA_TVOVER:
                TV = pViaModeTable->vt1622aOverTable[tvIndx].TVPAL;
                RGB = pViaModeTable->vt1622aOverTable[tvIndx].RGBPAL;
                YCbCr = pViaModeTable->vt1622aOverTable[tvIndx].YCbCrPAL;
                Patch2 = pViaModeTable->vt1622aOverTable[tvIndx].PatchPAL2;
                break;
        }
    }
    else {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                TV = pViaModeTable->vt1622aTable[tvIndx].TVNTSC;
                DotCrawl = pViaModeTable->vt1622aTable[tvIndx].DotCrawlNTSC;
                RGB = pViaModeTable->vt1622aTable[tvIndx].RGBNTSC;
                YCbCr = pViaModeTable->vt1622aTable[tvIndx].YCbCrNTSC;
                Patch2 = pViaModeTable->vt1622aTable[tvIndx].PatchNTSC2;
                break;
            case VIA_TVOVER:
                TV = pViaModeTable->vt1622aOverTable[tvIndx].TVNTSC;
                DotCrawl = pViaModeTable->vt1622aOverTable[tvIndx].DotCrawlNTSC;
                RGB = pViaModeTable->vt1622aOverTable[tvIndx].RGBNTSC;
                YCbCr = pViaModeTable->vt1622aOverTable[tvIndx].YCbCrNTSC;
                Patch2 = pViaModeTable->vt1622aOverTable[tvIndx].PatchNTSC2;
                break;
        }
    }

    for (i = 0, j = 0; i < TVMaskTbl->numTV
	     && j < VIA_BIOS_MAX_NUM_TV_REG ; j++) {
        if (TVMaskTbl->TV[j] == 0xFF) {
            W_Buffer[j+1] = TV[j];
            i++;
        }
    }
    w_bytes = j + 1;

    VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
    /* TV Reset */
    VIAGPIOI2C_Write(pBIOSInfo, 0x1D, 0x0);
    VIAGPIOI2C_Write(pBIOSInfo, 0x1D, 0x80);

    for (i = 0; i < w_bytes && i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        VIAGPIOI2C_Write(pBIOSInfo, i, W_Buffer[i+1]);
    }

    /* Turn on all Composite and S-Video output */
    VIAGPIOI2C_Write(pBIOSInfo, 0x0E, 0x0);

    if (pBIOSInfo->TVDotCrawl && (tvType == TVTYPE_NTSC)) {
        int numReg = (int)(DotCrawl[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(DotCrawl[i] & 0xFF);
            if (W_Other[0] == 0x11) {
                VIAGPIOI2C_Read(pBIOSInfo, 0x11, R_Buffer, 1);
                W_Other[1] = R_Buffer[0] | (unsigned char)(DotCrawl[i] >> 8);
            }
            else {
                W_Other[1] = (unsigned char)(DotCrawl[i] >> 8);
            }
            VIAGPIOI2C_Write(pBIOSInfo, W_Other[0], W_Other[1]);
        }
    }

    if (pBIOSInfo->TVOutput == TVOUTPUT_RGB) {
        int numReg = (int)(RGB[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(RGB[i] & 0xFF);
            W_Other[1] = (unsigned char)(RGB[i] >> 8);
            VIAGPIOI2C_Write(pBIOSInfo, W_Other[0], W_Other[1]);
        }

    }

    if (pBIOSInfo->TVOutput == TVOUTPUT_YCBCR) {
        int numReg = (int)(YCbCr[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(YCbCr[i] & 0xFF);
            W_Other[1] = (unsigned char)(YCbCr[i] >> 8);
            VIAGPIOI2C_Write(pBIOSInfo, W_Other[0], W_Other[1]);
        }

    }

    if (pBIOSInfo->IsSecondary) {
        int numPatch;

        /* Patch as setting 2nd path */
        numPatch = (int)(TVMaskTbl->misc2 >> 5);
        for (i = 0; i < numPatch; i++) {
            W_Other[0] = (unsigned char)(Patch2[i] & 0xFF);
            W_Other[1] = (unsigned char)(Patch2[i] >> 8);
            VIAGPIOI2C_Write(pBIOSInfo, W_Other[0], W_Other[1]);
        }
    }
}

void VIAPostSetCH7019Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           *CRTC1, *CRTC2, *Misc1, *Misc2, tmp;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j, data;

    pVia = pBIOSInfo;
    CRTC1 = NULL;
    CRTC2 = NULL;
    Misc1 = NULL;
    Misc2 = NULL;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPostSetCH7019Mode\n"));

    if (pBIOSInfo->TVType == TVTYPE_PAL) {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                CRTC1 = pViaModeTable->ch7019Table[tvIndx].CRTCPAL1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->ch7019Table[tvIndx].CRTCPAL2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->ch7019Table[tvIndx].CRTCPAL2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->ch7019Table[tvIndx].CRTCPAL2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->ch7019Table[tvIndx].MiscPAL1;
                Misc2 = pViaModeTable->ch7019Table[tvIndx].MiscPAL2;
                break;
            case VIA_TVOVER:
                CRTC1 = pViaModeTable->ch7019OverTable[tvIndx].CRTCPAL1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->ch7019OverTable[tvIndx].CRTCPAL2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->ch7019OverTable[tvIndx].CRTCPAL2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->ch7019OverTable[tvIndx].CRTCPAL2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->ch7019OverTable[tvIndx].MiscPAL1;
                Misc2 = pViaModeTable->ch7019OverTable[tvIndx].MiscPAL2;
                break;
        }
    }
    else {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                CRTC1 = pViaModeTable->ch7019Table[tvIndx].CRTCNTSC1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->ch7019Table[tvIndx].CRTCNTSC2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->ch7019Table[tvIndx].CRTCNTSC2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->ch7019Table[tvIndx].CRTCNTSC2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->ch7019Table[tvIndx].MiscNTSC1;
                Misc2 = pViaModeTable->ch7019Table[tvIndx].MiscNTSC2;
                break;
            case VIA_TVOVER:
                CRTC1 = pViaModeTable->ch7019OverTable[tvIndx].CRTCNTSC1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->ch7019OverTable[tvIndx].CRTCNTSC2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->ch7019OverTable[tvIndx].CRTCNTSC2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->ch7019OverTable[tvIndx].CRTCNTSC2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->ch7019OverTable[tvIndx].MiscNTSC1;
                Misc2 = pViaModeTable->ch7019OverTable[tvIndx].MiscNTSC2;
                break;
        }
    }

    if (pBIOSInfo->IsSecondary) {
        for (i = 0, j = 0; i < pViaModeTable->ch7019MaskTable.numCRTC2; j++) {
            if (pViaModeTable->ch7019MaskTable.CRTC2[j] == 0xFF) {
                VGAOUT8(0x3d4, j + 0x50);
                VGAOUT8(0x3d5, CRTC2[j]);
                i++;
            }
        }

        j = 3;

        if (pViaModeTable->ch7019MaskTable.misc2 & 0x18) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            VGAOUT8(0x3c4, 0x44);
            VGAOUT8(0x3c5, Misc2[j++]);
            VGAOUT8(0x3c4, 0x45);
            VGAOUT8(0x3c5, Misc2[j++]);

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x04);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFB);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }

        VGAOUT8(0x3d4, 0x6A);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0xC0);
        VGAOUT8(0x3d4, 0x6B);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);
        VGAOUT8(0x3d4, 0x6C);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);

        /* Disable LCD Scaling */
        if (!pBIOSInfo->SAMM || pBIOSInfo->FirstInit) {
            VGAOUT8(0x3d4, 0x79);
            VGAOUT8(0x3d5, 0);
        }
    }
    else {
        for (i = 0, j = 0; i < pViaModeTable->ch7019MaskTable.numCRTC1; j++) {
            if (pViaModeTable->ch7019MaskTable.CRTC1[j] == 0xFF) {
                VGAOUT8(0x3d4, j);
                VGAOUT8(0x3d5, CRTC1[j]);
                i++;
            }
        }

        j = 0;

        VGAOUT8(0x3d4, 0x33);
	tmp = VGAIN8(0x3d5);
        if (Misc1[j] & 0x20) {
            VGAOUT8(0x3d5, tmp | 0x20);
            j++;
        }
        else {
            VGAOUT8(0x3d5, tmp & 0xdf);
            j++;
        }
        VGAOUT8(0x3d4, 0x6A);
        VGAOUT8(0x3d5, Misc1[j++]);
        VGAOUT8(0x3d4, 0x6B);
        VGAOUT8(0x3d5, Misc1[j++] | 0x01);

        if (!pBIOSInfo->A2) {
            VGAOUT8(0x3d4, 0x6C);
            VGAOUT8(0x3d5, (Misc1[j++] & 0xE1) | 0x01);
        }
        else {
            VGAOUT8(0x3d4, 0x6C);
            VGAOUT8(0x3d5, Misc1[j++] | 0x01);
        }

        if (pViaModeTable->ch7019MaskTable.misc1 & 0x30) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            VGAOUT8(0x3c4, 0x46);
            VGAOUT8(0x3c5, Misc1[j++]);
            VGAOUT8(0x3c4, 0x47);
            VGAOUT8(0x3c5, Misc1[j++]);

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x02);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFD);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }
    }
}

void VIAPostSetFS454Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           *CRTC1, *Misc1, tmp;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j, data;


    pVia = pBIOSInfo;
    CRTC1 = NULL;
    Misc1 = NULL;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPostSetFS454Mode\n"));
    switch (pBIOSInfo->TVVScan) {
        case VIA_TVNORMAL:
            CRTC1 = pViaModeTable->fs454Table[tvIndx].CRTCNTSC1;
            Misc1 = pViaModeTable->fs454Table[tvIndx].MiscNTSC1;
            break;
        case VIA_TVOVER:
            CRTC1 = pViaModeTable->fs454OverTable[tvIndx].CRTCNTSC1;
            Misc1 = pViaModeTable->fs454OverTable[tvIndx].MiscNTSC1;
            break;
    }

    for (i = 0, j = 0; i < pViaModeTable->fs454MaskTable.numCRTC1; j++) {
        if (pViaModeTable->fs454MaskTable.CRTC1[j] == 0xFF) {
            VGAOUT8(0x3d4, j);
            VGAOUT8(0x3d5, CRTC1[j]);
            i++;
        }
    }

    j = 0;

    VGAOUT8(0x3d4, 0x33);
    tmp = VGAIN8(0x3d5);
    if (Misc1[j] & 0x20) {
        VGAOUT8(0x3d5, tmp | 0x20);
        j++;
    }
    else {
        VGAOUT8(0x3d5, tmp & 0xdf);
        j++;
    }
    VGAOUT8(0x3d4, 0x6A);
    VGAOUT8(0x3d5, Misc1[j++]);
    VGAOUT8(0x3d4, 0x6B);
    VGAOUT8(0x3d5, Misc1[j++] | 0x01);

    if (!pBIOSInfo->A2) {
        VGAOUT8(0x3d4, 0x6C);
        VGAOUT8(0x3d5, (Misc1[j++] & 0xE1) | 0x01);
    }
    else {
        VGAOUT8(0x3d4, 0x6C);
        VGAOUT8(0x3d5, Misc1[j++] | 0x01);
    }

    if (pViaModeTable->fs454MaskTable.misc1 & 0x30) {
        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp & 0x7F);

        VGAOUT8(0x3c4, 0x46);
        VGAOUT8(0x3c5, Misc1[j++]);
        VGAOUT8(0x3c4, 0x47);
        VGAOUT8(0x3c5, Misc1[j++]);

        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x80);

        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp | 0x02);
        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp & 0xFD);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }
}

void VIAPostSetSAA7108Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           *CRTC1, *CRTC2, *Misc1, *Misc2, tmp;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j, data;


    pVia = pBIOSInfo;
    CRTC1 = NULL;
    CRTC2 = NULL;
    Misc1 = NULL;
    Misc2 = NULL;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPostSetSAA7108Mode\n"));
    if (pBIOSInfo->TVType == TVTYPE_PAL) {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                CRTC1 = pViaModeTable->saa7108Table[tvIndx].CRTCPAL1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->saa7108Table[tvIndx].CRTCPAL2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->saa7108Table[tvIndx].CRTCPAL2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->saa7108Table[tvIndx].CRTCPAL2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->saa7108Table[tvIndx].MiscPAL1;
                Misc2 = pViaModeTable->saa7108Table[tvIndx].MiscPAL2;
                break;
            case VIA_TVOVER:
                CRTC1 = pViaModeTable->saa7108OverTable[tvIndx].CRTCPAL1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->saa7108OverTable[tvIndx].CRTCPAL2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->saa7108OverTable[tvIndx].CRTCPAL2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->saa7108OverTable[tvIndx].CRTCPAL2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->saa7108OverTable[tvIndx].MiscPAL1;
                Misc2 = pViaModeTable->saa7108OverTable[tvIndx].MiscPAL2;
                break;
        }
    }
    else {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                CRTC1 = pViaModeTable->saa7108Table[tvIndx].CRTCNTSC1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->saa7108Table[tvIndx].CRTCNTSC2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->saa7108Table[tvIndx].CRTCNTSC2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->saa7108Table[tvIndx].CRTCNTSC2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->saa7108Table[tvIndx].MiscNTSC1;
                Misc2 = pViaModeTable->saa7108Table[tvIndx].MiscNTSC2;
                break;
            case VIA_TVOVER:
                CRTC1 = pViaModeTable->saa7108OverTable[tvIndx].CRTCNTSC1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->saa7108OverTable[tvIndx].CRTCNTSC2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->saa7108OverTable[tvIndx].CRTCNTSC2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->saa7108OverTable[tvIndx].CRTCNTSC2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->saa7108OverTable[tvIndx].MiscNTSC1;
                Misc2 = pViaModeTable->saa7108OverTable[tvIndx].MiscNTSC2;
                break;
        }
    }

    if (pBIOSInfo->IsSecondary) {
        for (i = 0, j = 0; i < pViaModeTable->saa7108MaskTable.numCRTC2; j++) {
            if (pViaModeTable->saa7108MaskTable.CRTC2[j] == 0xFF) {
                VGAOUT8(0x3d4, j + 0x50);
                VGAOUT8(0x3d5, CRTC2[j]);
                i++;
            }
        }

        j = 3;

        if (pViaModeTable->saa7108MaskTable.misc2 & 0x18) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            VGAOUT8(0x3c4, 0x44);
            VGAOUT8(0x3c5, Misc2[j++]);
            VGAOUT8(0x3c4, 0x45);
            VGAOUT8(0x3c5, Misc2[j++]);

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x04);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFB);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }

        VGAOUT8(0x3d4, 0x6A);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0xC0);
        VGAOUT8(0x3d4, 0x6B);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);
        VGAOUT8(0x3d4, 0x6C);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);

        /* Disable LCD Scaling */
        if (!pBIOSInfo->SAMM || pBIOSInfo->FirstInit) {
            VGAOUT8(0x3d4, 0x79);
            VGAOUT8(0x3d5, 0);
        }
    }
    else {
        for (i = 0, j = 0; i < pViaModeTable->saa7108MaskTable.numCRTC1; j++) {
            if (pViaModeTable->saa7108MaskTable.CRTC1[j] == 0xFF) {
                VGAOUT8(0x3d4, j);
                VGAOUT8(0x3d5, CRTC1[j]);
                i++;
            }
        }

        j = 0;

        VGAOUT8(0x3d4, 0x33);
	tmp = VGAIN8(0x3d5);
        if (Misc1[j] & 0x20) {
            VGAOUT8(0x3d5, tmp | 0x20);
            j++;
        }
        else {
            VGAOUT8(0x3d5, tmp & 0xdf);
            j++;
        }

        VGAOUT8(0x3d4, 0x6A);
        VGAOUT8(0x3d5, Misc1[j++]);
        VGAOUT8(0x3d4, 0x6B);
        VGAOUT8(0x3d5, Misc1[j++] | 0x01);

        if (!pBIOSInfo->A2) {
            VGAOUT8(0x3d4, 0x6C);
            VGAOUT8(0x3d5, (Misc1[j++] & 0xE1) | 0x01);
        }
        else {
            VGAOUT8(0x3d4, 0x6C);
            VGAOUT8(0x3d5, Misc1[j++] | 0x01);
        }

        if (pViaModeTable->saa7108MaskTable.misc1 & 0x30) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            VGAOUT8(0x3c4, 0x46);
            VGAOUT8(0x3c5, Misc1[j++]);
            VGAOUT8(0x3c4, 0x47);
            VGAOUT8(0x3c5, Misc1[j++]);

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x02);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFD);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }
    }
}

void VIAPostSetTV2Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8           *CRTC1, *CRTC2, *Misc1, *Misc2, tmp;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j, data;


    pVia = pBIOSInfo;
    CRTC1 = NULL;
    CRTC2 = NULL;
    Misc1 = NULL;
    Misc2 = NULL;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPostSetTV2Mode\n"));
    if (pBIOSInfo->TVType == TVTYPE_PAL) {
        switch (pBIOSInfo->TVVScan) {
            case VIA_TVNORMAL:
                CRTC1 = pViaModeTable->tv2Table[tvIndx].CRTCPAL1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->tv2Table[tvIndx].CRTCPAL2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->tv2Table[tvIndx].CRTCPAL2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->tv2Table[tvIndx].CRTCPAL2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->tv2Table[tvIndx].MiscPAL1;
                Misc2 = pViaModeTable->tv2Table[tvIndx].MiscPAL2;
                break;
            case VIA_TVOVER:
                CRTC1 = pViaModeTable->tv2OverTable[tvIndx].CRTCPAL1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->tv2OverTable[tvIndx].CRTCPAL2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->tv2OverTable[tvIndx].CRTCPAL2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->tv2OverTable[tvIndx].CRTCPAL2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->tv2OverTable[tvIndx].MiscPAL1;
                Misc2 = pViaModeTable->tv2OverTable[tvIndx].MiscPAL2;
                break;
        }
    }
        else {
            switch (pBIOSInfo->TVVScan) {
                case VIA_TVNORMAL:
                CRTC1 = pViaModeTable->tv2Table[tvIndx].CRTCNTSC1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->tv2Table[tvIndx].CRTCNTSC2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->tv2Table[tvIndx].CRTCNTSC2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->tv2Table[tvIndx].CRTCNTSC2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->tv2Table[tvIndx].MiscNTSC1;
                Misc2 = pViaModeTable->tv2Table[tvIndx].MiscNTSC2;
                break;
            case VIA_TVOVER:
                CRTC1 = pViaModeTable->tv2OverTable[tvIndx].CRTCNTSC1;
                switch (pBIOSInfo->bitsPerPixel) {
                    case 8:
                        CRTC2 = pViaModeTable->tv2OverTable[tvIndx].CRTCNTSC2_8BPP;
                        break;
                    case 16:
                        CRTC2 = pViaModeTable->tv2OverTable[tvIndx].CRTCNTSC2_16BPP;
                        break;
                    case 24:
                    case 32:
                        CRTC2 = pViaModeTable->tv2OverTable[tvIndx].CRTCNTSC2_32BPP;
                        break;
                }
                Misc1 = pViaModeTable->tv2OverTable[tvIndx].MiscNTSC1;
                Misc2 = pViaModeTable->tv2OverTable[tvIndx].MiscNTSC2;
                break;
        }
    }

    if (pBIOSInfo->IsSecondary) {
        for (i = 0, j = 0; i < pViaModeTable->tv2MaskTable.numCRTC2; j++) {
            if (pViaModeTable->tv2MaskTable.CRTC2[j] == 0xFF) {
                VGAOUT8(0x3d4, j + 0x50);
                VGAOUT8(0x3d5, CRTC2[j]);
                i++;
            }
        }

        j = 3;

        if (pViaModeTable->tv2MaskTable.misc2 & 0x18) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            VGAOUT8(0x3c4, 0x44);
            VGAOUT8(0x3c5, Misc2[j++]);
            VGAOUT8(0x3c4, 0x45);
            VGAOUT8(0x3c5, Misc2[j++]);

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x04);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFB);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }

        VGAOUT8(0x3d4, 0x6A);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0xC0);
        VGAOUT8(0x3d4, 0x6B);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);
        VGAOUT8(0x3d4, 0x6C);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);

        /* Disable LCD Scaling */
    	if (!pBIOSInfo->SAMM || pBIOSInfo->FirstInit) {
	        VGAOUT8(0x3d4, 0x79);
	        VGAOUT8(0x3d5, 0);
	    }
    }
    else {
        for (i = 0, j = 0; i < pViaModeTable->tv2MaskTable.numCRTC1; j++) {
            if (pViaModeTable->tv2MaskTable.CRTC1[j] == 0xFF) {
                VGAOUT8(0x3d4, j);
                VGAOUT8(0x3d5, CRTC1[j]);
                i++;
            }
        }

        j = 0;

        VGAOUT8(0x3d4, 0x33);
	tmp = VGAIN8(0x3d5);
        if (Misc1[j] & 0x20) {
            VGAOUT8(0x3d5, tmp | 0x20);
            j++;
        }
        else {
            VGAOUT8(0x3d5, tmp & 0xdf);
            j++;
        }

        VGAOUT8(0x3d4, 0x6A);
        VGAOUT8(0x3d5, Misc1[j++]);
        VGAOUT8(0x3d4, 0x6B);
        VGAOUT8(0x3d5, Misc1[j++] | 0x01);
        VGAOUT8(0x3d4, 0x6C);
        VGAOUT8(0x3d5, Misc1[j++] | 0x01);

        if (pViaModeTable->tv2MaskTable.misc1 & 0x30) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            VGAOUT8(0x3c4, 0x46);
            VGAOUT8(0x3c5, Misc1[j++]);
            VGAOUT8(0x3c4, 0x47);
            VGAOUT8(0x3c5, Misc1[j++]);

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x02);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFD);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }
    }
}

void VIAPreSetTV3Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    VIABIOSTVMASKTablePtr TVMaskTbl;
    CARD8           *TV;
    CARD16          *DotCrawl, *RGB, *YCbCr, *Patch2;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             tvType = pBIOSInfo->TVType;
    int             i, j;
    unsigned char   W_Buffer[VIA_BIOS_MAX_NUM_TV_REG + 1];
    unsigned char   W_Other[2];
    int             w_bytes;
    I2CDevPtr       dev;
    unsigned char   R_Buffer[1];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPreSetTV3Mode\n"));
    DotCrawl = NULL;
    RGB = NULL;
    YCbCr = NULL;
    Patch2 = NULL;
    TV = NULL;
    W_Buffer[0] = 0;
    for (i = 0; i < VIA_BIOS_MAX_NUM_TV_REG; i++) {
        W_Buffer[i+1] = pBIOSInfo->TVRegs[i];
    }

	if (pBIOSInfo->TVEncoder == VIA_TV3) {
		TVMaskTbl = &pViaModeTable->tv3MaskTable;
	    if (tvType == TVTYPE_PAL) {
	        switch (pBIOSInfo->TVVScan) {
	            case VIA_TVNORMAL:
	                TV = pViaModeTable->tv3Table[tvIndx].TVPAL;
	                RGB = pViaModeTable->tv3Table[tvIndx].RGBPAL;
	                YCbCr = pViaModeTable->tv3Table[tvIndx].YCbCrPAL;
	                Patch2 = pViaModeTable->tv3Table[tvIndx].PatchPAL2;
	                break;
	            case VIA_TVOVER:
	                TV = pViaModeTable->tv3OverTable[tvIndx].TVPAL;
	                RGB = pViaModeTable->tv3OverTable[tvIndx].RGBPAL;
	                YCbCr = pViaModeTable->tv3OverTable[tvIndx].YCbCrPAL;
	                Patch2 = pViaModeTable->tv3OverTable[tvIndx].PatchPAL2;
	                break;
	        }
	    }
        else {
            switch (pBIOSInfo->TVVScan) {
                case VIA_TVNORMAL:
	                TV = pViaModeTable->tv3Table[tvIndx].TVNTSC;
	                DotCrawl = pViaModeTable->tv3Table[tvIndx].DotCrawlNTSC;
	                RGB = pViaModeTable->tv3Table[tvIndx].RGBNTSC;
	                YCbCr = pViaModeTable->tv3Table[tvIndx].YCbCrNTSC;
	                Patch2 = pViaModeTable->tv3Table[tvIndx].PatchNTSC2;
	                break;
	            case VIA_TVOVER:
	                TV = pViaModeTable->tv3OverTable[tvIndx].TVNTSC;
	                DotCrawl = pViaModeTable->tv3OverTable[tvIndx].DotCrawlNTSC;
	                RGB = pViaModeTable->tv3OverTable[tvIndx].RGBNTSC;
	                YCbCr = pViaModeTable->tv3OverTable[tvIndx].YCbCrNTSC;
	                Patch2 = pViaModeTable->tv3OverTable[tvIndx].PatchNTSC2;
	                break;
	        }
	    }
	}
	else {
		TVMaskTbl = &pViaModeTable->vt1622aMaskTable;
	    if (tvType == TVTYPE_PAL) {
	        switch (pBIOSInfo->TVVScan) {
	            case VIA_TVNORMAL:
	                TV = pViaModeTable->vt1622aTable[tvIndx].TVPAL;
	                RGB = pViaModeTable->vt1622aTable[tvIndx].RGBPAL;
	                YCbCr = pViaModeTable->vt1622aTable[tvIndx].YCbCrPAL;
	                Patch2 = pViaModeTable->vt1622aTable[tvIndx].PatchPAL2;
	                break;
	            case VIA_TVOVER:
	                TV = pViaModeTable->vt1622aOverTable[tvIndx].TVPAL;
	                RGB = pViaModeTable->vt1622aOverTable[tvIndx].RGBPAL;
	                YCbCr = pViaModeTable->vt1622aOverTable[tvIndx].YCbCrPAL;
	                Patch2 = pViaModeTable->vt1622aOverTable[tvIndx].PatchPAL2;
	                break;
	        }
	    }
	    else {
	        switch (pBIOSInfo->TVVScan) {
	            case VIA_TVNORMAL:
	                TV = pViaModeTable->vt1622aTable[tvIndx].TVNTSC;
	                DotCrawl = pViaModeTable->vt1622aTable[tvIndx].DotCrawlNTSC;
	                RGB = pViaModeTable->vt1622aTable[tvIndx].RGBNTSC;
	                YCbCr = pViaModeTable->vt1622aTable[tvIndx].YCbCrNTSC;
	                Patch2 = pViaModeTable->vt1622aTable[tvIndx].PatchNTSC2;
	                break;
	            case VIA_TVOVER:
	                TV = pViaModeTable->vt1622aOverTable[tvIndx].TVNTSC;
	                DotCrawl = pViaModeTable->vt1622aOverTable[tvIndx].DotCrawlNTSC;
	                RGB = pViaModeTable->vt1622aOverTable[tvIndx].RGBNTSC;
	                YCbCr = pViaModeTable->vt1622aOverTable[tvIndx].YCbCrNTSC;
	                Patch2 = pViaModeTable->vt1622aOverTable[tvIndx].PatchNTSC2;
	                break;
	        }
	    }
	}

    for (i = 0, j = 0; i < TVMaskTbl->numTV
	     && j < VIA_BIOS_MAX_NUM_TV_REG; j++) {
        if (TVMaskTbl->TV[j] == 0xFF) {
            W_Buffer[j+1] = TV[j];
            i++;
        }
    }
    w_bytes = j + 1;

    dev = xf86CreateI2CDevRec();
    dev->DevName = "VT1622";
    dev->SlaveAddr = 0x40;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;

    xf86I2CDevInit(dev);

    /* TV Reset */
    W_Other[0] = 0x1D;
    W_Other[1] = 0;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);
    W_Other[0] = 0x1D;
    W_Other[1] = 0x80;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);

    xf86I2CWriteRead(dev, W_Buffer,w_bytes, NULL,0);

    /* Turn on all Composite and S-Video output */
    W_Other[0] = 0x0E;
    W_Other[1] = 0;
    xf86I2CWriteRead(dev, W_Other,2, NULL,0);

    if (pBIOSInfo->TVDotCrawl && (tvType == TVTYPE_NTSC)) {
        int numReg = (int)(DotCrawl[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(DotCrawl[i] & 0xFF);
            if (W_Other[0] == 0x11) {
                xf86I2CWriteRead(dev, W_Other,1, R_Buffer,1);
                W_Other[1] = R_Buffer[0] | (unsigned char)(DotCrawl[i] >> 8);
            }
            else {
                W_Other[1] = (unsigned char)(DotCrawl[i] >> 8);
            }
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
    }

    if (pBIOSInfo->TVOutput == TVOUTPUT_RGB) {
        int numReg = (int)(RGB[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(RGB[i] & 0xFF);
            W_Other[1] = (unsigned char)(RGB[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }

    }

    if (pBIOSInfo->TVOutput == TVOUTPUT_YCBCR) {
        int numReg = (int)(YCbCr[0]);

        for (i = 1; i < (numReg + 1); i++) {
            W_Other[0] = (unsigned char)(YCbCr[i] & 0xFF);
            W_Other[1] = (unsigned char)(YCbCr[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }

    }

    if (pBIOSInfo->IsSecondary) {
        int numPatch;

        /* Patch as setting 2nd path */
        numPatch = (int)(TVMaskTbl->misc2 >> 5);
        for (i = 0; i < numPatch; i++) {
            W_Other[0] = (unsigned char)(Patch2[i] & 0xFF);
            W_Other[1] = (unsigned char)(Patch2[i] >> 8);
            xf86I2CWriteRead(dev, W_Other,2, NULL,0);
        }
    }

    xf86DestroyI2CDevRec(dev,TRUE);
}

void VIAPostSetTV3Mode(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    VIABIOSTVMASKTablePtr TVMaskTbl;
    CARD8           *CRTC1, *CRTC2, *Misc1, *Misc2, tmp;
    unsigned int    tvIndx = pBIOSInfo->resTVMode;
    int             i, j, data;


    pVia = pBIOSInfo;
    CRTC1 = NULL;
    CRTC2 = NULL;
    Misc1 = NULL;
    Misc2 = NULL;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAPostSetTV3Mode\n"));
	if (pBIOSInfo->TVEncoder == VIA_TV3) {
		TVMaskTbl = &pViaModeTable->tv3MaskTable;
        if (pBIOSInfo->TVType == TVTYPE_PAL) {
            switch (pBIOSInfo->TVVScan) {
                case VIA_TVNORMAL:
	                CRTC1 = pViaModeTable->tv3Table[tvIndx].CRTCPAL1;
	                switch (pBIOSInfo->bitsPerPixel) {
	                    case 8:
	                        CRTC2 = pViaModeTable->tv3Table[tvIndx].CRTCPAL2_8BPP;
	                        break;
	                    case 16:
	                        CRTC2 = pViaModeTable->tv3Table[tvIndx].CRTCPAL2_16BPP;
	                        break;
	                    case 24:
	                    case 32:
	                        CRTC2 = pViaModeTable->tv3Table[tvIndx].CRTCPAL2_32BPP;
	                        break;
	                }
	                Misc1 = pViaModeTable->tv3Table[tvIndx].MiscPAL1;
	                Misc2 = pViaModeTable->tv3Table[tvIndx].MiscPAL2;
	                break;
	            case VIA_TVOVER:
	                CRTC1 = pViaModeTable->tv3OverTable[tvIndx].CRTCPAL1;
	                switch (pBIOSInfo->bitsPerPixel) {
	                    case 8:
	                        CRTC2 = pViaModeTable->tv3OverTable[tvIndx].CRTCPAL2_8BPP;
	                        break;
	                    case 16:
	                        CRTC2 = pViaModeTable->tv3OverTable[tvIndx].CRTCPAL2_16BPP;
	                        break;
	                    case 24:
	                    case 32:
	                        CRTC2 = pViaModeTable->tv3OverTable[tvIndx].CRTCPAL2_32BPP;
	                        break;
	                }
	                Misc1 = pViaModeTable->tv3OverTable[tvIndx].MiscPAL1;
	                Misc2 = pViaModeTable->tv3OverTable[tvIndx].MiscPAL2;
	                break;
	        }
	    }
        else {
            switch (pBIOSInfo->TVVScan) {
                case VIA_TVNORMAL:
	                CRTC1 = pViaModeTable->tv3Table[tvIndx].CRTCNTSC1;
	                switch (pBIOSInfo->bitsPerPixel) {
	                    case 8:
	                        CRTC2 = pViaModeTable->tv3Table[tvIndx].CRTCNTSC2_8BPP;
	                        break;
	                    case 16:
	                        CRTC2 = pViaModeTable->tv3Table[tvIndx].CRTCNTSC2_16BPP;
	                        break;
	                    case 24:
	                    case 32:
	                        CRTC2 = pViaModeTable->tv3Table[tvIndx].CRTCNTSC2_32BPP;
	                        break;
	                }
	                Misc1 = pViaModeTable->tv3Table[tvIndx].MiscNTSC1;
	                Misc2 = pViaModeTable->tv3Table[tvIndx].MiscNTSC2;
	                break;
	            case VIA_TVOVER:
	                CRTC1 = pViaModeTable->tv3OverTable[tvIndx].CRTCNTSC1;
	                switch (pBIOSInfo->bitsPerPixel) {
	                    case 8:
	                        CRTC2 = pViaModeTable->tv3OverTable[tvIndx].CRTCNTSC2_8BPP;
	                        break;
	                    case 16:
	                        CRTC2 = pViaModeTable->tv3OverTable[tvIndx].CRTCNTSC2_16BPP;
	                        break;
	                    case 24:
	                    case 32:
	                        CRTC2 = pViaModeTable->tv3OverTable[tvIndx].CRTCNTSC2_32BPP;
	                        break;
	                }
	                Misc1 = pViaModeTable->tv3OverTable[tvIndx].MiscNTSC1;
	                Misc2 = pViaModeTable->tv3OverTable[tvIndx].MiscNTSC2;
	                break;
	        }
	    }
	}
	else {
		TVMaskTbl = &pViaModeTable->vt1622aMaskTable;
	    if (pBIOSInfo->TVType == TVTYPE_PAL) {
	        switch (pBIOSInfo->TVVScan) {
	            case VIA_TVNORMAL:
	                CRTC1 = pViaModeTable->vt1622aTable[tvIndx].CRTCPAL1;
	                switch (pBIOSInfo->bitsPerPixel) {
	                    case 8:
	                        CRTC2 = pViaModeTable->vt1622aTable[tvIndx].CRTCPAL2_8BPP;
	                        break;
	                    case 16:
	                        CRTC2 = pViaModeTable->vt1622aTable[tvIndx].CRTCPAL2_16BPP;
	                        break;
	                    case 24:
	                    case 32:
	                        CRTC2 = pViaModeTable->vt1622aTable[tvIndx].CRTCPAL2_32BPP;
	                        break;
	                }
	                Misc1 = pViaModeTable->vt1622aTable[tvIndx].MiscPAL1;
	                Misc2 = pViaModeTable->vt1622aTable[tvIndx].MiscPAL2;
	                break;
	            case VIA_TVOVER:
	                CRTC1 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCPAL1;
	                switch (pBIOSInfo->bitsPerPixel) {
	                    case 8:
	                        CRTC2 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCPAL2_8BPP;
	                        break;
	                    case 16:
	                        CRTC2 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCPAL2_16BPP;
	                        break;
	                    case 24:
	                    case 32:
	                        CRTC2 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCPAL2_32BPP;
	                        break;
	                }
	                Misc1 = pViaModeTable->vt1622aOverTable[tvIndx].MiscPAL1;
	                Misc2 = pViaModeTable->vt1622aOverTable[tvIndx].MiscPAL2;
	                break;
	        }
	    }
	    else {
	        switch (pBIOSInfo->TVVScan) {
	            case VIA_TVNORMAL:
                    CRTC1 = pViaModeTable->vt1622aTable[tvIndx].CRTCNTSC1;
                    switch (pBIOSInfo->bitsPerPixel) {
                        case 8:
                            CRTC2 = pViaModeTable->vt1622aTable[tvIndx].CRTCNTSC2_8BPP;
                            break;
                        case 16:
                            CRTC2 = pViaModeTable->vt1622aTable[tvIndx].CRTCNTSC2_16BPP;
                            break;
                        case 24:
                        case 32:
                            CRTC2 = pViaModeTable->vt1622aTable[tvIndx].CRTCNTSC2_32BPP;
                            break;
                    }
                    Misc1 = pViaModeTable->vt1622aTable[tvIndx].MiscNTSC1;
                    Misc2 = pViaModeTable->vt1622aTable[tvIndx].MiscNTSC2;
                    break;
                case VIA_TVOVER:
                    CRTC1 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCNTSC1;
                    switch (pBIOSInfo->bitsPerPixel) {
                        case 8:
                            CRTC2 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCNTSC2_8BPP;
                            break;
                        case 16:
                            CRTC2 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCNTSC2_16BPP;
                            break;
                        case 24:
                        case 32:
                            CRTC2 = pViaModeTable->vt1622aOverTable[tvIndx].CRTCNTSC2_32BPP;
                            break;
                    }
                    Misc1 = pViaModeTable->vt1622aOverTable[tvIndx].MiscNTSC1;
                    Misc2 = pViaModeTable->vt1622aOverTable[tvIndx].MiscNTSC2;
                    break;
            }
        }
    }

    if (pBIOSInfo->IsSecondary) {
        for (i = 0, j = 0; i < TVMaskTbl->numCRTC2; j++) {
            if (TVMaskTbl->CRTC2[j] == 0xFF) {
                VGAOUT8(0x3d4, j + 0x50);
                VGAOUT8(0x3d5, CRTC2[j]);
                i++;
            }
        }

        j = 3;

        if (TVMaskTbl->misc2 & 0x18) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            /* CLE266Ax use 2x XCLK */
            if ((pBIOSInfo->Chipset == VIA_CLE266) &&
                (pBIOSInfo->ChipRev < 15)) {
                VGAOUT8(0x3d4, 0x6B);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp | 0x20);
                if (pBIOSInfo->A2) {
                    VGAOUT8(0x3d4, 0x6C);
		    tmp = VGAIN8(0x3d5);
                    VGAOUT8(0x3d5, tmp | 0x1C);
                }
                VGAOUT8(0x3c4, 0x44);
                VGAOUT8(0x3c5, 0x47);
                VGAOUT8(0x3c4, 0x45);
                VGAOUT8(0x3c5, 0x1C);
            }
            else {
                VGAOUT8(0x3c4, 0x44);
                VGAOUT8(0x3c5, Misc2[j++]);
                VGAOUT8(0x3c4, 0x45);
                VGAOUT8(0x3c5, Misc2[j++]);
            }

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x04);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFB);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }

        VGAOUT8(0x3d4, 0x6A);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0xC0);
        VGAOUT8(0x3d4, 0x6B);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);
        VGAOUT8(0x3d4, 0x6C);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);

        /* Disable LCD Scaling */
        if (!pBIOSInfo->SAMM || pBIOSInfo->FirstInit) {
            VGAOUT8(0x3d4, 0x79);
            VGAOUT8(0x3d5, 0);
        }
    }
    else {
        for (i = 0, j = 0; i < TVMaskTbl->numCRTC1; j++) {
            if (TVMaskTbl->CRTC1[j] == 0xFF) {
                VGAOUT8(0x3d4, j);
                VGAOUT8(0x3d5, CRTC1[j]);
                i++;
            }
        }

        j = 0;

        VGAOUT8(0x3d4, 0x33);
	tmp = VGAIN8(0x3d5);
        if (Misc1[j] & 0x20) {
            VGAOUT8(0x3d5, tmp | 0x20);
            j++;
        }
        else {
            VGAOUT8(0x3d5, tmp & 0xdf);
            j++;
        }
        VGAOUT8(0x3d4, 0x6A);
        VGAOUT8(0x3d5, Misc1[j++]);
        if ((pBIOSInfo->Chipset == VIA_CLE266) &&
            (pBIOSInfo->ChipRev < 15)) {
            VGAOUT8(0x3d4, 0x6B);
            VGAOUT8(0x3d5, Misc1[j++] | 0x81);
            if (pBIOSInfo->A2) {
                VGAOUT8(0x3d4, 0x6C);
                VGAOUT8(0x3d5, Misc1[j++] | 0x01);
            }
            else {
                j++;
            }
        }
        else {
            VGAOUT8(0x3d4, 0x6B);
            VGAOUT8(0x3d5, Misc1[j++] | 0x01);
            j++;
        }

        if (TVMaskTbl->misc1 & 0x30) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            /* CLE266Ax use 2x XCLK */
            if ((pBIOSInfo->Chipset == VIA_CLE266) &&
                (pBIOSInfo->ChipRev < 15)) {
                VGAOUT8(0x3c4, 0x46);
                VGAOUT8(0x3c5, 0x47);
                VGAOUT8(0x3c4, 0x47);
                VGAOUT8(0x3c5, 0x1C);
            }
            else {
                VGAOUT8(0x3c4, 0x46);
                VGAOUT8(0x3c5, Misc1[j++]);
                VGAOUT8(0x3c4, 0x47);
                VGAOUT8(0x3c5, Misc1[j++]);
            }

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x02);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFD);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }
        VGAOUT8(0x3d4, 0x6A);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x40);
        VGAOUT8(0x3d4, 0x6B);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);
        VGAOUT8(0x3d4, 0x6C);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x01);
    }
}

void VIADisableExtendedFIFO(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    CARD32          dwTemp;

    pVia = pBIOSInfo;

    dwTemp = (CARD32)VIAGETREG(0x298);
    dwTemp |= 0x20000000;
    VIASETREG(0x298, dwTemp);

    dwTemp = (CARD32)VIAGETREG(0x230);
    dwTemp &= ~0x00200000;
    VIASETREG(0x230, dwTemp);

    dwTemp = (CARD32)VIAGETREG(0x298);
    dwTemp &= ~0x20000000;
    VIASETREG(0x298, dwTemp);
}


void VIAEnableExtendedFIFO(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    CARD32          dwTemp;
    CARD8           bTemp;

    pVia = pBIOSInfo;

    dwTemp = (CARD32)VIAGETREG(0x298);
    dwTemp |= 0x20000000;
    VIASETREG(0x298, dwTemp);

    dwTemp = (CARD32)VIAGETREG(0x230);
    dwTemp |= 0x00200000;
    VIASETREG(0x230, dwTemp);

    dwTemp = (CARD32)VIAGETREG(0x298);
    dwTemp &= ~0x20000000;
    VIASETREG(0x298, dwTemp);

    VGAOUT8(0x3C4, 0x17);
    bTemp = VGAIN8(0x3C5);
    bTemp &= ~0x7F;
    bTemp |= 0x2F;
    VGAOUT8(0x3C5, bTemp);

    VGAOUT8(0x3C4, 0x16);
    bTemp = VGAIN8(0x3C5);
    bTemp &= ~0x3F;
    bTemp |= 0x17;
    VGAOUT8(0x3C5, bTemp);

    VGAOUT8(0x3C4, 0x18);
    bTemp = VGAIN8(0x3C5);
    bTemp &= ~0x3F;
    bTemp |= 0x17;
    bTemp |= 0x40; /* force the preq always higher than treq */
    VGAOUT8(0x3C5, bTemp);
}


Bool VIASetModeUseBIOSTable(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    VIAUserSettingPtr UserSetting = pBIOSInfo->UserSetting;
    BOOL            setTV = FALSE;
    int             mode, resMode, refresh;
    int             port, offset, mask, data;
    int             countWidthByQWord, offsetWidthByQWord;
    int             i, j, k, m, n;
    unsigned char   cr13, cr35, sr1c, sr1d;
    CARD8           misc, tmp;

    pVia = pBIOSInfo;
    mode = pBIOSInfo->mode;
    resMode = pBIOSInfo->resMode;
    refresh = pBIOSInfo->refresh;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASetModeUseBIOSTable\n"));
    /* Turn off Screen */
    VGAOUT8(0x3d4, 0x17);
    tmp = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, tmp & 0x7f);

    /* Clean Second Path Status */
#if 0
    if (pBIOSInfo->SAMM) {
        if (pBIOSInfo->FirstInit) {
            VGAOUT8(0x3d4, 0x6A);
            VGAOUT8(0x3d5, 0x40);
            if (pBIOSInfo->Chipset == VIA_CLE266 && pBIOSInfo->ChipRev < 15) {
                VGAOUT8(0x3d4, 0x6B);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp | 0x3E);
            }
            else {
                VGAOUT8(0x3d4, 0x6B);
		tmp = VGAIN8(0x3d5);
                VGAOUT8(0x3d5, tmp & 0x0E);
            }
            VGAOUT8(0x3d4, 0x6C);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0xFE);
            VGAOUT8(0x3d4, 0x93);
            VGAOUT8(0x3d5, 0x0);
        }
    }
    else {
        VGAOUT8(0x3d4, 0x6A);
        VGAOUT8(0x3d5, 0x0);
        VGAOUT8(0x3d4, 0x6B);
        VGAOUT8(0x3d5, 0x0);
        VGAOUT8(0x3d4, 0x6C);
        VGAOUT8(0x3d5, 0x0);
        VGAOUT8(0x3d4, 0x93);
        VGAOUT8(0x3d5, 0x0);
    }
#endif
    VGAOUT8(0x3d4, 0x6A);
    VGAOUT8(0x3d5, 0x0);
    VGAOUT8(0x3d4, 0x6B);
    VGAOUT8(0x3d5, 0x0);
    VGAOUT8(0x3d4, 0x6C);
    VGAOUT8(0x3d5, 0x0);
    VGAOUT8(0x3d4, 0x93);
    VGAOUT8(0x3d5, 0x0);

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "Active Device is %d\n",
                     pBIOSInfo->ActiveDevice));

    if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
        /* Found TV resmod index */
        switch (pBIOSInfo->resMode) {
            case VIA_RES_640X480 :
                pBIOSInfo->resTVMode = VIA_TVRES_640X480;
                break;
            case VIA_RES_800X600 :
                pBIOSInfo->resTVMode = VIA_TVRES_800X600;
                break;
            case VIA_RES_1024X768 :
                pBIOSInfo->resTVMode = VIA_TVRES_1024X768;
                break;
            case VIA_RES_848X480 :
                pBIOSInfo->resTVMode = VIA_TVRES_848X480;
                break;
            case VIA_RES_720X480 :
                pBIOSInfo->resTVMode = VIA_TVRES_720X480;
                pBIOSInfo->TVType = TVTYPE_NTSC;
                break;
            case VIA_RES_720X576 :
                pBIOSInfo->resTVMode = VIA_TVRES_720X576;
                pBIOSInfo->TVType = TVTYPE_PAL;
                break;
            default :
                pBIOSInfo->resTVMode = pBIOSInfo->resMode;
                break;
        }

        switch (pBIOSInfo->TVEncoder) {
            case VIA_TV2PLUS:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600) {
                    setTV = TRUE;
                    VIAPreSetTV2Mode(pBIOSInfo);
                }
                break;
            case VIA_TV3:
            case VIA_VT1622A:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
                    resMode == VIA_RES_1024X768 || resMode == VIA_RES_720X480 ||
                    resMode == VIA_RES_720X576 || resMode == VIA_RES_848X480) {
                    setTV = TRUE;
                    VIAPreSetTV3Mode(pBIOSInfo);
                }
                break;
            case VIA_VT1623:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
                    resMode == VIA_RES_1024X768 || resMode == VIA_RES_720X480 ||
                    resMode == VIA_RES_720X576 || resMode == VIA_RES_848X480) {
                    setTV = TRUE;
                    VIAPreSetVT1623Mode(pBIOSInfo);
                }
                break;
            case VIA_CH7009:
            case VIA_CH7019:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
                    resMode == VIA_RES_1024X768) {
                    setTV = TRUE;
                    VIAPreSetCH7019Mode(pBIOSInfo);
                }
                break;
            case VIA_SAA7108:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
                    resMode == VIA_RES_1024X768 || resMode == VIA_RES_848X480) {
                    setTV = TRUE;
                    VIAPreSetSAA7108Mode(pBIOSInfo);
                }
                break;
			case VIA_FS454:
				if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
             		resMode == VIA_RES_1024X768 || resMode == VIA_RES_720X480) {
					setTV = TRUE;
             		VIAPreSetFS454Mode(pBIOSInfo);
				}
				break;
        }
    }

    /* Close TV Output, if TVEncoder not support current resolution */
    if (!setTV && !pBIOSInfo->SAMM) {
        I2CDevPtr       dev;
        unsigned char   W_Buffer[2];

        dev = xf86CreateI2CDevRec();
        dev->DevName = "TV";
        dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;
        if (xf86I2CDevInit(dev)) {
            switch (pBIOSInfo->TVEncoder) {
                case VIA_TV2PLUS:
                    W_Buffer[0] = 0x0E;
                    W_Buffer[1] = 0x03;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
                case VIA_TV3:
                case VIA_VT1622A:
                    W_Buffer[0] = 0x0E;
                    W_Buffer[1] = 0x0F;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
                case VIA_VT1623:
                    VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
                    VIAGPIOI2C_Write(pBIOSInfo, 0x0E, 0x0F);
                    break;
                case VIA_CH7019:
                    W_Buffer[0] = 0x49;
                    W_Buffer[1] = 0x3E;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x1E;
                    W_Buffer[1] = 0xD0;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
                case VIA_SAA7108:
                    W_Buffer[0] = 0x2D;
                    W_Buffer[1] = 0x08;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
				case VIA_FS454:
                default:
                    break;
            }
            xf86DestroyI2CDevRec(dev,TRUE);
        }
        else
            xf86DestroyI2CDevRec(dev,TRUE);
    }

    i = mode;

    /* Get Standard VGA Regs */
    /* Set Sequences regs */
    for (j = 1; j < 5; j++) {
        VGAOUT8(0x3c4, j);
        VGAOUT8(0x3c5, pViaModeTable->Modes[i].stdVgaTable.SR[j]);
    }

    /* Set Misc reg */
    VGAOUT8(0x3c2, pViaModeTable->Modes[i].stdVgaTable.misc);

    /* Set CRTC regs */
    for (j = 0; j < 25; j++) {
        VGAOUT8(0x3d4, j);
        VGAOUT8(0x3d5, pViaModeTable->Modes[i].stdVgaTable.CR[j]);
    }

    /* Set attribute regs */
    for (j = 0; j < 20; j++) {
        misc = VGAIN8(0x3da);
        VGAOUT8(0x3c0, j);
        VGAOUT8(0x3c0, pViaModeTable->Modes[i].stdVgaTable.AR[j]);
    }

    for (j = 0; j < 9; j++) {
        VGAOUT8(0x3ce, j);
        VGAOUT8(0x3cf, pViaModeTable->Modes[i].stdVgaTable.GR[j]);
    }

    /* Unlock Extended regs */
    VGAOUT8(0x3c4, 0x10);
    VGAOUT8(0x3c5, 0x01);

    /* Set Commmon Ext. Regs */
    for (j = 0; j < pViaModeTable->commExtTable.numEntry; j++) {
        port = pViaModeTable->commExtTable.port[j];
        offset = pViaModeTable->commExtTable.offset[j];
        mask = pViaModeTable->commExtTable.mask[j];
        data = pViaModeTable->commExtTable.data[j] & mask;
        VGAOUT8(0x300+port, offset);
        VGAOUT8(0x301+port, data);
    }

    if (pViaModeTable->Modes[i].Mode <= 0x13) {
        /* Set Standard Mode-Spec. Extend Regs */
        for (j = 0; j < pViaModeTable->stdModeExtTable.numEntry; j++) {
            port = pViaModeTable->stdModeExtTable.port[j];
            offset = pViaModeTable->stdModeExtTable.offset[j];
            mask = pViaModeTable->stdModeExtTable.mask[j];
            data = pViaModeTable->stdModeExtTable.data[j] & mask;
            VGAOUT8(0x300+port, offset);
            VGAOUT8(0x301+port, data);
        }
    }
    else {
        /* Set Extended Mode-Spec. Extend Regs */
        for (j = 0; j < pViaModeTable->Modes[i].extModeExtTable.numEntry; j++) {
            port = pViaModeTable->Modes[i].extModeExtTable.port[j];
            offset = pViaModeTable->Modes[i].extModeExtTable.offset[j];
            mask = pViaModeTable->Modes[i].extModeExtTable.mask[j];
            data = pViaModeTable->Modes[i].extModeExtTable.data[j] & mask;
            VGAOUT8(0x300+port, offset);
            VGAOUT8(0x301+port, data);
        }
    }

    j = resMode;

    if ((j != VIA_RES_INVALID) && (refresh != 0xFF) && !setTV) {
        k = refresh;

        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp & 0x7F);

        VGAOUT8(0x3c4, 0x46);
        VGAOUT8(0x3c5, (pViaModeTable->refreshTable[j][k].VClk >> 8));
        VGAOUT8(0x3c4, 0x47);
        VGAOUT8(0x3c5, (pViaModeTable->refreshTable[j][k].VClk & 0xFF));

        VGAOUT8(0x3d4, 0x17);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x80);

        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp | 0x02);
        VGAOUT8(0x3c4, 0x40);
	tmp = VGAIN8(0x3c5);
        VGAOUT8(0x3c5, tmp & 0xFD);

        m = 0;
        VGAOUT8(0x3d4, 0x0);
        VGAOUT8(0x3d5, pViaModeTable->refreshTable[j][k].CR[m++]);

        for (n = 2; n <= 7; n++) {
            VGAOUT8(0x3d4, n);
            VGAOUT8(0x3d5, pViaModeTable->refreshTable[j][k].CR[m++]);
        }

        for (n = 16; n <= 17; n++) {
            VGAOUT8(0x3d4, n);
            VGAOUT8(0x3d5, pViaModeTable->refreshTable[j][k].CR[m++]);
        }

        for (n = 21; n <= 22; n++) {
            VGAOUT8(0x3d4, n);
            VGAOUT8(0x3d5, pViaModeTable->refreshTable[j][k].CR[m++]);
        }

        /* Use external clock */
        data = VGAIN8(0x3cc) | 0x0C;
        VGAOUT8(0x3c2, data);

    }
    else {
        if (pViaModeTable->Modes[i].VClk != 0) {
            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0x7F);

            VGAOUT8(0x3c4, 0x46);
            VGAOUT8(0x3c5, (pViaModeTable->Modes[i].VClk >> 8));
            VGAOUT8(0x3c4, 0x47);
            VGAOUT8(0x3c5, (pViaModeTable->Modes[i].VClk & 0xFF));

            VGAOUT8(0x3d4, 0x17);
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x80);

            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp | 0x02);
            VGAOUT8(0x3c4, 0x40);
	    tmp = VGAIN8(0x3c5);
            VGAOUT8(0x3c5, tmp & 0xFD);

            /* Use external clock */
            data = VGAIN8(0x3cc) | 0x0C;
            VGAOUT8(0x3c2, data);
        }
    }

    /* Set Quadword offset and counter */
    /*=* Fix bandwidth problem when using virtual desktop *=*/
    countWidthByQWord = pBIOSInfo->countWidthByQWord;
    offsetWidthByQWord = pBIOSInfo->offsetWidthByQWord;

    VGAOUT8(0x3d4, 0x35);
    cr35 = VGAIN8(0x3d5) & 0x1f;

    cr13 = offsetWidthByQWord & 0xFF;
    cr35 |= (offsetWidthByQWord & 0x700) >> 3;        /* bit 7:5: offset 10:8 */

    VGAOUT8(0x3c4, 0x1d);
    sr1d = VGAIN8(0x3c5);

    /* Patch for non 32bit alignment mode */
    VIAPitchAlignmentPatch(pBIOSInfo);

    /* Enable Refresh to avoid data lose when enter screen saver */
    /* Refresh disable & 128-bit alignment */
    sr1d = (sr1d & 0xfc) | (countWidthByQWord >> 9);
    sr1c = ((countWidthByQWord >> 1) + 1) & 0xff;
    /* sr1d = ((sr1d & 0xfc) | (widthByQWord >> 8)) | 0x80; */
    /* sr1c = widthByQWord & 0xff; */

    VGAOUT8(0x3d4, 0x13);
    VGAOUT8(0x3d5, cr13);
    VGAOUT8(0x3d4, 0x35);
    VGAOUT8(0x3d5, cr35);
    VGAOUT8(0x3c4, 0x1c);
    VGAOUT8(0x3c5, sr1c);
    VGAOUT8(0x3c4, 0x1d);
    VGAOUT8(0x3c5, sr1d);

    /* Enable MMIO & PCI burst (1 wait state) */
    VGAOUT8(0x3c4, 0x1a);
    data = VGAIN8(0x3c5);
    VGAOUT8(0x3c5, data | 0x06);

    /* Enable modify CRTC starting address */
    VGAOUT8(0x3d4, 0x11);
    misc = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, misc & 0x7f);

    if (pBIOSInfo->FirstInit) {
        /* Clear Memory */
        memset(pBIOSInfo->FBBase, 0x00, pBIOSInfo->videoRambytes);
        /*pBIOSInfo->FirstInit = FALSE;*/
    }

    /*=* Patch for horizontal blanking end bit6 *=*/
    VGAOUT8(0x3d4, 0x02);
    data = VGAIN8(0x3d5); /* Save Blanking Start */

    VGAOUT8(0x3d4, 0x03);
    misc = VGAIN8(0x3d5) & 0x1f; /* Save Blanking End bit[4:0] */

    VGAOUT8(0x3d4, 0x05);
    misc |= ((VGAIN8(0x3d5) & 0x80) >> 2); /* Blanking End bit[5:0] */

    if ((data & 0x3f) > misc) { /* Is Blanking End overflow ? */
        if (data & 0x40) { /* Blanking Start bit6 = ? */
            VGAOUT8(0x3d4, 0x33); /* bit6 = 1, Blanking End bit6 = 0 */
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0xdf);
        }
        else {
            VGAOUT8(0x3d4, 0x33); /* bit6 = 0, Blanking End bit6 = 1 */
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x20);
        }
    }
    else {
        if (data & 0x40) { /* Blanking Start bit6 = ? */
            VGAOUT8(0x3d4, 0x33); /* bit6 = 1, Blanking End bit6 = 1 */
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp | 0x20);
        }
        else {
            VGAOUT8(0x3d4, 0x33); /* bit6 = 0, Blanking End bit6 = 0 */
	    tmp = VGAIN8(0x3d5);
            VGAOUT8(0x3d5, tmp & 0xdf);
        }
    }

    /* LCD Simultaneous Set Mode */
    if (pBIOSInfo->ActiveDevice & (VIA_DEVICE_DFP | VIA_DEVICE_LCD)) {
        VIASetLCDMode(pBIOSInfo);
        VIAEnableLCD(pBIOSInfo);
    }
    else if ((pBIOSInfo->ConnectedDevice & (VIA_DEVICE_DFP | VIA_DEVICE_LCD)) &&
             (!pBIOSInfo->HasSecondary)) {
        VIADisableLCD(pBIOSInfo);
    }

    if (setTV) {
        switch (pBIOSInfo->TVEncoder) {
            case VIA_TV2PLUS:
                VIAPostSetTV2Mode(pBIOSInfo);
                break;
            case VIA_TV3:
            case VIA_VT1622A:
            case VIA_VT1623:
                VIAPostSetTV3Mode(pBIOSInfo);
                break;
            case VIA_CH7009:
            case VIA_CH7019:
                VIAPostSetCH7019Mode(pBIOSInfo);
                break;
            case VIA_SAA7108:
                VIAPostSetSAA7108Mode(pBIOSInfo);
                break;
	    case VIA_FS454:
		VIAPostSetFS454Mode(pBIOSInfo);
		break;
        }
    }

    /* load/save TV attribute for utility. */
    if (!pBIOSInfo->HasSecondary && (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV)) {
        /* Read User Setting */
        if (UserSetting->DefaultSetting) {
            VIARestoreUserSetting(pBIOSInfo);
        }
        else {                                      /* Read from I2C */
            VIAUTGetInfo(pBIOSInfo);
        }
    }

    VIAEnabledPrimaryExtendedFIFO(pBIOSInfo);

    /* Enable CRT Controller (3D5.17 Hardware Reset) */
    VGAOUT8(0x3d4, 0x17);
    misc = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, (misc | 0x80));

    /* Turn off CRT, if user doesn't want crt on */
    if (!(pBIOSInfo->ActiveDevice & VIA_DEVICE_CRT1)) {
        VGAOUT8(0x3d4, 0x36);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp | 0x30);
    }

    /* Open Screen */
    misc = VGAIN8(0x3da);
    VGAOUT8(0x3c0, 0x20);

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "-- VIASetModeUseBIOSTable Done!\n"));
    return TRUE;
}

Bool VIASetModeForMHS(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia;
    BOOL            setTV = FALSE;
    int             resMode;
    int             countWidthByQWord, offsetWidthByQWord;
    unsigned char   cr65, cr66, cr67, tmp;

    pVia = pBIOSInfo;
    resMode = pBIOSInfo->resMode;
    pBIOSInfo->SetTV = FALSE;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIASetModeForMHS\n"));
    /* Turn off Screen */
    VGAOUT8(0x3d4, 0x17);
    tmp = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, tmp & 0x7f);

    if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
        /* Found TV resmod index */
        switch (pBIOSInfo->resMode) {
            case VIA_RES_640X480 :
                pBIOSInfo->resTVMode = VIA_TVRES_640X480;
                break;
            case VIA_RES_800X600 :
                pBIOSInfo->resTVMode = VIA_TVRES_800X600;
                break;
            case VIA_RES_1024X768 :
                pBIOSInfo->resTVMode = VIA_TVRES_1024X768;
                break;
            case VIA_RES_848X480 :
                pBIOSInfo->resTVMode = VIA_TVRES_848X480;
                break;
            case VIA_RES_720X480 :
                pBIOSInfo->resTVMode = VIA_TVRES_720X480;
                pBIOSInfo->TVType = TVTYPE_NTSC;
                break;
            case VIA_RES_720X576 :
                pBIOSInfo->resTVMode = VIA_TVRES_720X576;
                pBIOSInfo->TVType = TVTYPE_PAL;
                break;
            default :
                pBIOSInfo->resTVMode = pBIOSInfo->resMode;
                break;
        }
        switch (pBIOSInfo->TVEncoder) {
            case VIA_TV2PLUS:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600) {
                    setTV = TRUE;
                    pBIOSInfo->SetTV = TRUE;
                    VIAPreSetTV2Mode(pBIOSInfo);
                    VIAPostSetTV2Mode(pBIOSInfo);
                }
                break;
            case VIA_TV3:
            case VIA_VT1622A:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
                    resMode == VIA_RES_1024X768 || resMode == VIA_RES_720X480 ||
                    resMode == VIA_RES_720X576 || resMode == VIA_RES_848X480) {
                    setTV = TRUE;
                    pBIOSInfo->SetTV = TRUE;
                    VIAPreSetTV3Mode(pBIOSInfo);
                    VIAPostSetTV3Mode(pBIOSInfo);
                }
                break;
            case VIA_CH7009:
            case VIA_CH7019:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
                    resMode == VIA_RES_1024X768) {
                    setTV = TRUE;
                    pBIOSInfo->SetTV = TRUE;
                    VIAPreSetCH7019Mode(pBIOSInfo);
                    VIAPostSetCH7019Mode(pBIOSInfo);
                }
                break;
            case VIA_SAA7108:
                if (resMode == VIA_RES_640X480 || resMode == VIA_RES_800X600 ||
                    resMode == VIA_RES_1024X768 || resMode == VIA_RES_848X480) {
                    setTV = TRUE;
                    pBIOSInfo->SetTV = TRUE;
                    VIAPreSetSAA7108Mode(pBIOSInfo);
                    VIAPostSetSAA7108Mode(pBIOSInfo);
                }
                break;
			case VIA_FS454:
				return FALSE;
				break;
        }
    }

    /* Close TV Output, if TVEncoder not support current resolution */
    if ((pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) && (!setTV)) {
        I2CDevPtr       dev;
        unsigned char   W_Buffer[2];

        dev = xf86CreateI2CDevRec();
        dev->DevName = "TV";
        dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;
        if (xf86I2CDevInit(dev)) {
            switch (pBIOSInfo->TVEncoder) {
                case VIA_TV2PLUS:
                    W_Buffer[0] = 0x0E;
                    W_Buffer[1] = 0x03;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
                case VIA_TV3:
                case VIA_VT1622A:
                    W_Buffer[0] = 0x0E;
                    W_Buffer[1] = 0x0F;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
                case VIA_CH7019:
                    W_Buffer[0] = 0x49;
                    W_Buffer[1] = 0x3E;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    W_Buffer[0] = 0x1E;
                    W_Buffer[1] = 0xD0;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
                case VIA_SAA7108:
                    W_Buffer[0] = 0x2D;
                    W_Buffer[1] = 0x08;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
				case VIA_FS454:
                default:
                    break;
            }
            xf86DestroyI2CDevRec(dev,TRUE);
        }
        else
            xf86DestroyI2CDevRec(dev,TRUE);
        ErrorF("TV resolution not supported!!\n");
    }

    if (!pBIOSInfo->A2) {
        VGAOUT8(0x3d4, 0x6C);
	tmp = VGAIN8(0x3d5);
        VGAOUT8(0x3d5, tmp & 0xE1);
    }

    if (pBIOSInfo->ActiveDevice & (VIA_DEVICE_DFP | VIA_DEVICE_LCD)) {
        pBIOSInfo->SetDVI = TRUE;
        VIASetLCDMode(pBIOSInfo);
        VIAEnableLCD(pBIOSInfo);
    }
    else if (pBIOSInfo->ConnectedDevice & (VIA_DEVICE_DFP | VIA_DEVICE_LCD)) {
        VIADisableLCD(pBIOSInfo);
    }


    /* Set Extended FIFO & Expire Number */
    VIAEnabledSecondaryExtendedFIFO(pBIOSInfo);
    VIAFillExpireNumber(pBIOSInfo);
    /* Set Quadword offset and counter */
    countWidthByQWord = pBIOSInfo->countWidthByQWord;
    offsetWidthByQWord = pBIOSInfo->offsetWidthByQWord;

    VGAOUT8(0x3d4, 0x67);
    cr67 = VGAIN8(0x3d5) & 0xFC;

    cr66 = offsetWidthByQWord & 0xFF;
    cr67 |= (offsetWidthByQWord & 0x300) >> 8;

    VGAOUT8(0x3d4, 0x66);
    VGAOUT8(0x3d5, cr66);
    VGAOUT8(0x3d4, 0x67);
    VGAOUT8(0x3d5, cr67);

    VGAOUT8(0x3d4, 0x67);
    cr67 = VGAIN8(0x3d5) & 0xF3;

    cr67 |= (countWidthByQWord & 0x600) >> 7;
    cr65 = (countWidthByQWord >> 1) & 0xff;

    VGAOUT8(0x3d4, 0x65);
    VGAOUT8(0x3d5, cr65);
    VGAOUT8(0x3d4, 0x67);
    VGAOUT8(0x3d5, cr67);

    VGAOUT8(0x3d4, 0x67);
    cr67 = VGAIN8(0x3d5);

    switch (pBIOSInfo->bitsPerPixel) {
        case 8:
            cr67 &= 0x3F;
            VGAOUT8(0x3d5, cr67);
            break;
        case 16:
            cr67 &= 0x3F;
            cr67 |= 0x40;
            VGAOUT8(0x3d5, cr67);
            break;
        case 24:
        case 32:
            cr67 |= 0x80;
            VGAOUT8(0x3d5, cr67);
            break;
    }

    /* Patch for non 32bit alignment mode */
    VIAPitchAlignmentPatch(pBIOSInfo);

    /* Open Screen */
    (void) VGAIN8(0x3da);
    VGAOUT8(0x3c0, 0x20);

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "-- VIASetModeForMHS Done!\n"));
    return TRUE;
}

Bool VIASavePalette(ScrnInfoPtr pScrn, LOCO *colors) {
    VIAPtr pVia = VIAPTR(pScrn);
    int i, sr1a, sr1b, cr67, cr6a;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIASavePalette\n"));
    /*VGAOUT8(0x3C4, 0x16);
    sr16 = VGAIN8(0x3C5);*/
    VGAOUT8(0x3C4, 0x1A);
    sr1a = VGAIN8(0x3C5);
    VGAOUT8(0x3C4, 0x1B);
    sr1b = VGAIN8(0x3C5);
    VGAOUT8(0x3D4, 0x67);
    cr67 = VGAIN8(0x3D5);
    VGAOUT8(0x3D4, 0x6A);
    cr6a = VGAIN8(0x3D5);
    /*VGAOUT8(0x3C4, 0x16);
    if (pScrn->bitsPerPixel == 8)
        VGAOUT8(0x3C5, sr16 & ~0x80);
    else
        VGAOUT8(0x3C5, sr16 | 0x80);*/
    if (pVia->IsSecondary) {
        VGAOUT8(0x3C4, 0x1A);
        VGAOUT8(0x3C5, sr1a | 0x01);
        VGAOUT8(0x3C4, 0x1B);
        VGAOUT8(0x3C5, sr1b | 0x80);
        VGAOUT8(0x3D4, 0x67);
        VGAOUT8(0x3D5, cr67 & 0x3F);
        VGAOUT8(0x3D4, 0x6A);
        VGAOUT8(0x3D5, cr6a | 0xC0);
    }
    else {
        VGAOUT8(0x3C4, 0x1A);
        VGAOUT8(0x3C5, sr1a & 0xFE);
        VGAOUT8(0x3C4, 0x1B);
        VGAOUT8(0x3C5, sr1b | 0x20);
    }

    VGAOUT8(0x3c7, 0);
    for (i = 0; i < 256; i++) {
        colors[i].red = VGAIN8(0x3c9);
        colors[i].green = VGAIN8(0x3c9);
        colors[i].blue = VGAIN8(0x3c9);
        DEBUG(xf86Msg(X_INFO, "%d, %d, %d\n", colors[i].red, colors[i].green, colors[i].blue));
    }
    WaitIdle();

    /*if (pScrn->bitsPerPixel == 8) {
        VGAOUT8(0x3C4, 0x16);
        VGAOUT8(0x3C5, sr16);
    }
    VGAOUT8(0x3C4, 0x16);
    VGAOUT8(0x3C5, sr16);*/
    VGAOUT8(0x3C4, 0x1A);
    VGAOUT8(0x3C5, sr1a);
    VGAOUT8(0x3C4, 0x1B);
    VGAOUT8(0x3C5, sr1b);

    if (pVia->IsSecondary) {
        VGAOUT8(0x3D4, 0x67);
        VGAOUT8(0x3D5, cr67);
        VGAOUT8(0x3D4, 0x6A);
        VGAOUT8(0x3D5, cr6a);
    }
    return TRUE;
}

Bool VIARestorePalette(ScrnInfoPtr pScrn, LOCO *colors) {
    VIAPtr pVia = VIAPTR(pScrn);
    int i, sr1a, sr1b, cr67, cr6a, sr16;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIARestorePalette\n"));
    VGAOUT8(0x3C4, 0x16);
    sr16 = VGAIN8(0x3C5);
    VGAOUT8(0x3C4, 0x1A);
    sr1a = VGAIN8(0x3C5);
    VGAOUT8(0x3C4, 0x1B);
    sr1b = VGAIN8(0x3C5);
    VGAOUT8(0x3D4, 0x67);
    cr67 = VGAIN8(0x3D5);
    VGAOUT8(0x3D4, 0x6A);
    cr6a = VGAIN8(0x3D5);
    VGAOUT8(0x3C4, 0x16);
    if (pScrn->bitsPerPixel == 8)
        VGAOUT8(0x3C5, sr16 & ~0x80);
    else
        VGAOUT8(0x3C5, sr16 | 0x80);
    if (pVia->IsSecondary) {
        VGAOUT8(0x3C4, 0x1A);
        VGAOUT8(0x3C5, sr1a | 0x01);
        VGAOUT8(0x3C4, 0x1B);
        VGAOUT8(0x3C5, sr1b | 0x80);
        VGAOUT8(0x3D4, 0x67);
        VGAOUT8(0x3D5, cr67 & 0x3F);
        VGAOUT8(0x3D4, 0x6A);
        VGAOUT8(0x3D5, cr6a | 0xC0);
    }
    else {
        VGAOUT8(0x3C4, 0x1A);
        VGAOUT8(0x3C5, sr1a & 0xFE);
        VGAOUT8(0x3C4, 0x1B);
        VGAOUT8(0x3C5, sr1b | 0x20);
    }

    VGAOUT8(0x3c8, 0);
    for (i = 0; i < 256; i++) {
        VGAOUT8(0x3c9, colors[i].red);
        VGAOUT8(0x3c9, colors[i].green);
        VGAOUT8(0x3c9, colors[i].blue);
        /*DEBUG(xf86Msg(X_INFO, "%d, %d, %d\n", colors[i].red, colors[i].green, colors[i].blue));*/
    }
    WaitIdle();
    /*if (pScrn->bitsPerPixel == 8) {
        VGAOUT8(0x3C4, 0x16);
        VGAOUT8(0x3C5, sr16);
    }
    VGAOUT8(0x3C4, 0x16);
    VGAOUT8(0x3C5, sr16);*/
    VGAOUT8(0x3C4, 0x1A);
    VGAOUT8(0x3C5, sr1a);
    VGAOUT8(0x3C4, 0x1B);
    VGAOUT8(0x3C5, sr1b);

    if (pVia->IsSecondary) {
        VGAOUT8(0x3D4, 0x67);
        VGAOUT8(0x3D5, cr67);
        VGAOUT8(0x3D4, 0x6A);
        VGAOUT8(0x3D5, cr6a);
    }
    return TRUE;
}

void VIAPitchAlignmentPatch(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    CARD8      cr13, cr35, cr65, cr66, cr67;
    CARD32     dwScreenPitch= 0;
    CARD32     dwPitch;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "VIAPitchAlignmentPatch\n"));
    dwPitch = pBIOSInfo->HDisplay * (pBIOSInfo->bitsPerPixel >> 3);
    if (dwPitch & 0x1F) {   /* Is 32 Byte Alignment ? */
        dwScreenPitch = ((dwPitch + 31) & ~31) >> 3;
        if (!pBIOSInfo->IsSecondary) {
            cr13 = (CARD8)(dwScreenPitch & 0xFF);
            VGAOUT8(0x3D4, 0x13);
            VGAOUT8(0x3D5, cr13);
            VGAOUT8(0x3D4, 0x35);
            cr35 = VGAIN8(0x3D5) & 0x1F;
            cr35 |= (CARD8)((dwScreenPitch & 0x700) >> 3);
            VGAOUT8(0x3D5, cr35);
        }
        else {
            cr66 = (CARD8)(dwScreenPitch & 0xFF);
            VGAOUT8(0x3D4, 0x66);
            VGAOUT8(0x3D5, cr66);
            VGAOUT8(0x3D4, 0x67);
            cr67 = VGAIN8(0x3D5) & 0xFC;
            cr67 |= (CARD8)((dwScreenPitch & 0x300) >> 8);
            VGAOUT8(0x3D5, cr67);

            /* Fetch Count */
            VGAOUT8(0x3D4, 0x67);
            cr67 = VGAIN8(0x3D5) & 0xF3;
            cr67 |= (CARD8)((dwScreenPitch & 0x600) >> 7);
            VGAOUT8(0x3D5, cr67);
            cr65 = (CARD8)((dwScreenPitch >> 1) & 0xFF);
            cr65 += 2;
            VGAOUT8(0x3D4, 0x65);
            VGAOUT8(0x3D5, cr65);
        }
    }
}

void VIAEnableLCD(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    I2CDevPtr       dev;
    unsigned char   W_Buffer[2], R_Buffer[1];
    unsigned char   cr6a;
    int             i, j, k;
    int             port, offset, mask, data;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "VIAEnableLCD\n"));
    /* Enable LCD */
    VGAOUT8(0x3d4, 0x6A);
    cr6a = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, cr6a | 0x08);

    /* Find Panel Size Index for PowerSeq Table */
    if (pBIOSInfo->Chipset == VIA_CLE266) {
        if (pBIOSInfo->PanelSize != VIA_RES_INVALID) {
            for (i = 0; i < VIA_BIOS_NUM_PANEL; i++) {
                if (pViaModeTable->lcdTable[i].fpSize == pBIOSInfo->PanelSize)
                    break;
            }

            for (j = 0; j < pViaModeTable->NumPowerOn; j++) {
                if (pViaModeTable->lcdTable[i].powerSeq ==
                    pViaModeTable->powerOn[j].powerSeq)
                    break;
            }
        }
        else {
            j = 0;
        }
    }
    else {  /* KM and K8M use PowerSeq Table index 2. */
        j = 2;
    }

    usleep(1);
    for (k = 0; k < pViaModeTable->powerOn[j].numEntry; k++) {
        port = pViaModeTable->powerOn[j].port[k];
        offset = pViaModeTable->powerOn[j].offset[k];
        mask = pViaModeTable->powerOn[j].mask[k];
        data = pViaModeTable->powerOn[j].data[k] & mask;
        VGAOUT8(0x300+port, offset);
        VGAOUT8(0x301+port, data);
        usleep(pViaModeTable->powerOn[j].delay[k]);
    }
    usleep(1);

    /* Patch for CH7019LVDS PLL Lock */
    if (pBIOSInfo->LVDS == VIA_CH7019LVDS) {
        dev = xf86CreateI2CDevRec();
        dev->DevName = "LVDS";
        dev->SlaveAddr = 0xEA;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;
        if (xf86I2CDevInit(dev)) {
            W_Buffer[0] = 0x63;
            W_Buffer[1] = 0x4B;
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
            W_Buffer[0] = 0x66;
            W_Buffer[1] = 0x20;
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
            for (i = 0; i < 10; i++) {
                W_Buffer[0] = 0x63;
                xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                usleep(100);
                W_Buffer[0] = 0x63;
                W_Buffer[1] = (R_Buffer[0] | 0x40);
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "[%d]write 0x63 = %X!\n", i+1, W_Buffer[1]));
                usleep(1);
                W_Buffer[0] = 0x63;
                W_Buffer[1] &= ~0x40;
                xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "[%d]write 0x63 = %X!\n", i+1, W_Buffer[1]));
                usleep(100);
                W_Buffer[0] = 0x66;
                xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                if (((R_Buffer[0] & 0x44) == 0x44) || (i >= 9)) {  /* PLL lock OK, Turn on VDD */
                    usleep(500);
                    W_Buffer[1] = R_Buffer[0] | 0x01;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "CH7019 PLL lock ok!\n"));
                    /* reset data path */
                    W_Buffer[0] = 0x48;
                    xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
                    W_Buffer[1] = R_Buffer[0] & ~0x08;
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    usleep(1);
                    W_Buffer[1] = R_Buffer[0];
                    xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
                    break;
                }
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "[%d]CH7019 PLL lock fail!\n", i+1));
                DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "[%d]0x66 = %X!\n", i+1, R_Buffer[0]));
            }
            xf86DestroyI2CDevRec(dev,TRUE);
        }
    }
}

void VIADisableLCD(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    I2CDevPtr       dev;
    unsigned char   W_Buffer[2], R_Buffer[1];
    unsigned char   cr6a, tmp;
    int             i, j, k;
    int             port, offset, mask, data;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_PROBED, "VIADisableLCD\n"));
    /* Patch for CH7019LVDS PLL Lock */
    if (pBIOSInfo->LVDS == VIA_CH7019LVDS) {
        dev = xf86CreateI2CDevRec();
        dev->DevName = "LVDS";
        dev->SlaveAddr = 0xEA;
        dev->pI2CBus = pBIOSInfo->I2C_Port2;
        if (xf86I2CDevInit(dev)) {
            /* Turn off VDD (Turn off backlignt only) */
            W_Buffer[0] = 0x66;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
            W_Buffer[1] &= ~0x01;
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);
            usleep(100);
            /* Turn off LVDS path */
            W_Buffer[0] = 0x63;
            xf86I2CWriteRead(dev, W_Buffer,1, R_Buffer,1);
            W_Buffer[1] = (R_Buffer[0] | 0x40);
            xf86I2CWriteRead(dev, W_Buffer,2, NULL,0);

            xf86DestroyI2CDevRec(dev,TRUE);
        }
    }

    /* Disable LCD */
    VGAOUT8(0x3d4, 0x6A);
    cr6a = VGAIN8(0x3d5);
    VGAOUT8(0x3d5, (cr6a & ~0x08));

    /* Find Panel Size Index */
    for (i = 0; i < VIA_BIOS_NUM_PANEL; i++) {
        if (pViaModeTable->lcdTable[i].fpSize == pBIOSInfo->PanelSize)
            break;
    }

    for (j = 0; j < pViaModeTable->NumPowerOn; j++) {
        if (pViaModeTable->lcdTable[i].powerSeq ==
            pViaModeTable->powerOn[j].powerSeq)
            break;
    }

    for (k = 0; k < pViaModeTable->powerOff[j].numEntry; k++) {
        port = pViaModeTable->powerOff[j].port[k];
        offset = pViaModeTable->powerOff[j].offset[k];
        mask = pViaModeTable->powerOff[j].mask[k];
        data = pViaModeTable->powerOff[j].data[k] & mask;
        VGAOUT8(0x300+port, offset);
	tmp = VGAIN8(0x301+port);
        VGAOUT8(0x301+port, (tmp & ~mask) | data);
        usleep(pViaModeTable->powerOff[j].delay[k]);
     }
}

unsigned char VIABIOS_GetActiveDevice(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;
    unsigned char   ret = 0x0;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetActiveDevice\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0xFF;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x0103;
    pVia->pInt10->cx = 0;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xFF) != 0x4F) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "BIOS Get Active Device fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0xFF;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    if (pVia->pInt10->cx & 0x01)
        ret = VIA_DEVICE_CRT1;
    if (pVia->pInt10->cx & 0x02)
        ret |= VIA_DEVICE_LCD;
    if (pVia->pInt10->cx & 0x04)
        ret |= VIA_DEVICE_TV;
    if (pVia->pInt10->cx & 0x20)
        ret |= VIA_DEVICE_DFP;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Return Value Is: %u\n", ret));
    return ret;
}

/*=*
 *
 * unsigned char VIABIOS_GetDisplayDeviceInfo(pScrn, *numDevice)
 *
 *     - Get Display Device Information
 *
 * Return Type:    unsigned int
 *
 * The Definition of Input Value:
 *
 *                 ScrnInfoPtr
 *                 point of int    0-CRT
 *                                 1-DVI
 *                                 2-LCD Panel
 *
 * The Definition of Return Value:
 *
 *                 Bit[15:0]    Max. vertical resolution
 *=*/

unsigned int VIABIOS_GetDisplayDeviceInfo(ScrnInfoPtr pScrn, unsigned char *numDevice)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetDisplayDeviceInfo\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0xFFFF;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x0806;
    pVia->pInt10->cx = *numDevice;
    pVia->pInt10->di = 0x00;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xFF) != 0x4F) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "BIOS Get Device Info fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0xFFFF;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    *numDevice = pVia->pInt10->cx;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Vertical Resolution Is: %u\n", pVia->pInt10->di & 0xFFFF));
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel ID Is: %u\n", *numDevice));
    return (pVia->pInt10->di & 0xFFFF);
}

/*=*
 *
 * unsigned char VIABIOS_GetDisplayDeviceAttached(pScrn)
 *
 *     - Get Display Device Information
 *
 * Return Type:    unsigned char
 *
 * The Definition of Input Value:
 *
 *                 ScrnInfoPtr
 *
 * The Definition of Return Value:
 *
 *                 Bit[4]    CRT2
 *                 Bit[3]    DFP
 *                 Bit[2]    TV
 *                 Bit[1]    LCD
 *                 Bit[0]    CRT
 *=*/

unsigned char VIABIOS_GetDisplayDeviceAttached(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;
    unsigned char   ret = 0;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetDisplayDeviceAttached\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0xFF;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x0004;
    pVia->pInt10->cx = 0x00;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xFF) != 0x4F) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "BIOS Get Display Device Attached fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0xFF;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    if (pVia->pInt10->cx & 0x01)
        ret = VIA_DEVICE_CRT1;
    if (pVia->pInt10->cx & 0x02)
        ret |= VIA_DEVICE_LCD;
    if (pVia->pInt10->cx & 0x04)
        ret |= VIA_DEVICE_TV;
    if (pVia->pInt10->cx & 0x20)
        ret |= VIA_DEVICE_DFP;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Connected Device Is: %d\n", ret));
    return ret;
}

Bool VIABIOS_GetBIOSDate(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    int RealOff;
    pointer page = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetBIOSDate\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return FALSE;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x0100;
    pVia->pInt10->cx = 0;
    pVia->pInt10->dx = 0;
    pVia->pInt10->si = 0;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xff) != 0x4f) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Get BIOS Date fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return FALSE;
    }

    pBIOSInfo->BIOSDateYear = ((pVia->pInt10->bx >> 8) - 48) + ((pVia->pInt10->bx & 0xFF) - 48)*10;
    pBIOSInfo->BIOSDateMonth = ((pVia->pInt10->cx >> 8) - 48) + ((pVia->pInt10->cx & 0xFF) - 48)*10;
    pBIOSInfo->BIOSDateDay = ((pVia->pInt10->dx >> 8) - 48) + ((pVia->pInt10->dx & 0xFF) - 48)*10;

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIOS Release Date Is: %d/%d/%d\n"
     , pBIOSInfo->BIOSDateYear + 2000, pBIOSInfo->BIOSDateMonth, pBIOSInfo->BIOSDateDay));
    return TRUE;
}

int VIABIOS_GetBIOSVersion(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetBIOSVersion\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0xFFFF;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x0000;
    pVia->pInt10->cx = 0;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xff) != 0x4f) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Get BIOS Version fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0xFFFF;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Return Value Is: %u\n", pVia->pInt10->bx & 0xFFFF));
    return (pVia->pInt10->bx & 0xFFFF);
}

unsigned char VIABIOS_GetFlatPanelInfo(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetFlatPanelInfo\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0xFF;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x0006;
    pVia->pInt10->cx = 0x00;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xFF) != 0x4F) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "BIOS Get Flat Panel Info fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0xFF;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel ID Is: %u\n", pVia->pInt10->cx & 0x0F));
    return (pVia->pInt10->cx & 0x0F);
}

unsigned short VIABIOS_GetTVConfiguration(ScrnInfoPtr pScrn, CARD16 dx)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;
    unsigned short ret = 0;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetTVConfiguration\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0xFFFF;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x8107;
    pVia->pInt10->cx = 0x01;
    pVia->pInt10->dx = dx;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xff) != 0x4f) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Get TV Configuration fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0xFFFF;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    if (pVia->pInt10->dx)
        ret = VIABIOS_GetTVConfiguration(pScrn, pVia->pInt10->dx);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Return Value Is: %u\n", ret));
    return ret;
}

unsigned char VIABIOS_GetTVEncoderType(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetTVEncoderType\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0xFF;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x0000;
    pVia->pInt10->cx = 0;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xFF) != 0x4F) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Get TV Encoder Type fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0xFF;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Return Value Is: %u\n", pVia->pInt10->cx >> 8));
    return (pVia->pInt10->cx >> 8);
}

int VIABIOS_GetVideoMemSize(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    int RealOff;
    pointer page = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIABIOS_GetVideoMemSize\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return 0;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0;
    pVia->pInt10->cx = 0;
    pVia->pInt10->dx = 0;
    pVia->pInt10->di = 0;
    pVia->pInt10->si = 0;
    pVia->pInt10->num = 0x10;
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xFF) != 0x4F) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Get Video Memory Size fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return 0;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Return Value Is: %d\n", pVia->pInt10->si));
    if (pVia->pInt10->si > 1)
        return (pVia->pInt10->si);
    else
        return 0;
}

Bool VIABIOS_SetActiveDevice(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    int RealOff;
    pointer page = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIOS_SetActiveDevice\n"));
    page = xf86Int10AllocPages(pVia->pInt10, 1, &RealOff);
    if (!page)
        return FALSE;
    pVia->pInt10->ax = 0x4F14;
    pVia->pInt10->bx = 0x8003;
    pVia->pInt10->cx = 0;
    pVia->pInt10->dx = 0;
    pVia->pInt10->di = 0;
    pVia->pInt10->num = 0x10;

    /* Set Active Device and Translate BIOS byte definition */
    if (pBIOSInfo->ActiveDevice & VIA_DEVICE_CRT1)
        pVia->pInt10->cx = 0x01;
    if (pBIOSInfo->ActiveDevice & VIA_DEVICE_LCD)
        pVia->pInt10->cx |= 0x02;
    if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV)
        pVia->pInt10->cx |= 0x04;
    if (pBIOSInfo->ActiveDevice & VIA_DEVICE_DFP)
        pVia->pInt10->cx |= 0x20;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Active Device: %d\n",
                     pVia->pInt10->cx));

    /* Set Current mode */
    pVia->pInt10->dx = pViaModeTable->Modes[pBIOSInfo->mode].Mode;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Mode Nums: %d\n",
                     pVia->pInt10->dx));

    /* Set Current Refresh rate */
    switch(pBIOSInfo->Refresh) {
        case 60:
            pVia->pInt10->di = 0;
            break;
        case 75:
            pVia->pInt10->di = 5;
            break;
        case 85:
            pVia->pInt10->di = 7;
            break;
        case 100:
            pVia->pInt10->di = 9;
            break;
        case 120:
            pVia->pInt10->di = 10;
            break;
        default:
            pVia->pInt10->di = 0;
            break;
    }
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Refresh Rate Index: %d\n",
                     pVia->pInt10->di));

    /* Real execution */
    xf86ExecX86int10(pVia->pInt10);

    if ((pVia->pInt10->ax & 0xFF) != 0x4F) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "BIOS Set Active Device fail!\n"));
        if (page)
            xf86Int10FreePages(pVia->pInt10, page, 1);
        return FALSE;
    }

    if (page)
        xf86Int10FreePages(pVia->pInt10, page, 1);

    return TRUE;
}
