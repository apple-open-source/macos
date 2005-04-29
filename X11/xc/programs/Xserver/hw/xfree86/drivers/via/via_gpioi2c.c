/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_gpioi2c.c,v 1.3 2003/11/03 05:11:46 tsi Exp $ */
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

#include "via_driver.h"
#include "via_gpioi2c.h"

static void delayIn_usec(int usec);
static void HWGPIOI2C_SetSCL(VIABIOSInfoPtr pBIOSInfo, CARD8 flag);
static void HWGPIOI2C_SetSDA(VIABIOSInfoPtr pBIOSInfo, CARD8 flag);
static CARD8  HWGPIOI2C_GetSDA(VIABIOSInfoPtr pBIOSInfo);
static void GPIOI2C_START(VIABIOSInfoPtr pBIOSInfo);
static void GPIOI2C_STOP(VIABIOSInfoPtr pBIOSInfo);
static Bool GPIOI2C_ACKNOWLEDGE(VIABIOSInfoPtr pBIOSInfo);
static Bool GPIOI2C_SENDACKNOWLEDGE(VIABIOSInfoPtr pBIOSInfo);
static Bool GPIOI2C_SENDNACKNOWLEDGE(VIABIOSInfoPtr pBIOSInfo);
static Bool GPIOI2C_ReadBit(VIABIOSInfoPtr pBIOSInfo, int *psda, int timeout);
static CARD8 GPIOI2C_ReadData(VIABIOSInfoPtr pBIOSInfo);
static Bool GPIOI2C_WriteBit(VIABIOSInfoPtr pBIOSInfo, int sda, int timeout);
static Bool GPIOI2C_WriteData(VIABIOSInfoPtr pBIOSInfo, CARD8 Data);
static void I2C_RW_Control(VIABIOSInfoPtr pBIOSInfo, CARD8 Command, CARD8 Data);

/* I2C Functions */

Bool VIAGPIOI2C_Initial(VIABIOSInfoPtr pBIOSInfo, CARD8 SlaveDevice)
{
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "GPIOI2C_Initial\n"));
    switch (pBIOSInfo->Chipset)
    {
        case VIA_KM400:
        case VIA_K8M800:
            GPIOPORT = 0x2C;
            break;
        default:
            GPIOPORT = 0;
            DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO,
                              "GPIOI2C initial failure!\n"));
            return FALSE;
    }
/*
    switch (pBIOSInfo->TVEncoder)
    {
        case VIA_VT1623:
            break;
        default:
            SLAVEADDR = 0;
            return FALSE;
    }
*/
    if (SlaveDevice == 0xA0 || SlaveDevice == 0xA2) {
        I2C_WAIT_TIME = 40;
        STARTTIMEOUT = 550;
        BYTETIMEOUT = 2200;
        HOLDTIME = 5;
        BITTIMEOUT = 5;
    }
    else {
        I2C_WAIT_TIME = 5;
        STARTTIMEOUT = 5;
        BYTETIMEOUT = 5;
        HOLDTIME = 5;
        BITTIMEOUT = 5;
    }

    SLAVEADDR = SlaveDevice;

    return TRUE;
}

/* I2C R/W Control */
static void I2C_RW_Control(VIABIOSInfoPtr pBIOSInfo, CARD8 Command, CARD8 Data)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    CARD8   value;

    switch(Command) {
        case I2C_RELEASE:
            VGAOUT8(0x3c4, GPIOPORT);
            value = VGAIN8(0x3c5) & (CARD8)(~GPIOI2C_MASKD);
            VGAOUT8(0x3c5, value);
            break;
        case I2C_WRITE_SCL:
            VGAOUT8(0x3c4, GPIOPORT);
            value = VGAIN8(0x3c5) & (CARD8)(~GPIOI2C_SCL_MASK);
            value |= GPIOI2C_SCL_WRITE;
            if (Data) {
                VGAOUT8(0x3c5, (value | I2C_OUTPUT_CLOCK));
            }
            else {
                VGAOUT8(0x3c5, (value & (CARD8)(~I2C_OUTPUT_CLOCK)));
            }
            break;
        case I2C_READ_SCL:
            VGAOUT8(0x3c4, GPIOPORT);
            value = VGAIN8(0x3c5) & (CARD8)(~GPIOI2C_SCL_MASK);
            VGAOUT8(0x3c5, (value | GPIOI2C_SCL_READ));
            break;
        case I2C_WRITE_SDA:
            VGAOUT8(0x3c4, GPIOPORT);
            value = VGAIN8(0x3c5) & (CARD8)(~GPIOI2C_SDA_MASK);
            value |= GPIOI2C_SDA_WRITE;
            if (Data) {
                VGAOUT8(0x3c5, (value | I2C_OUTPUT_DATA));
            }
            else {
                VGAOUT8(0x3c5, (value & (CARD8)(~I2C_OUTPUT_DATA)));
            }
            break;
        case I2C_READ_SDA:
            VGAOUT8(0x3c4, GPIOPORT);
            value = VGAIN8(0x3c5) & (CARD8)(~GPIOI2C_SDA_MASK);
            VGAOUT8(0x3c5, (value | GPIOI2C_SCL_READ));
            break;
        default:
            break;
    }
}

static void delayIn_usec(int usec)
{
  long b_secs, b_usecs;
  long a_secs, a_usecs;
  long d_secs, d_usecs;
  long diff;

  if (usec > 0) {
    xf86getsecs(&b_secs, &b_usecs);
    do {
      /* It would be nice to use {xf86}usleep,
       * but usleep (1) takes >10000 usec !
       */
      xf86getsecs(&a_secs, &a_usecs);
      d_secs  = (a_secs - b_secs);
      d_usecs = (a_usecs - b_usecs);
      diff = d_secs*1000000 + d_usecs;
    } while (diff>0 && diff< (usec + 1));
  }
}

/* Set SCL */
static void HWGPIOI2C_SetSCL(VIABIOSInfoPtr pBIOSInfo, CARD8 flag)
{
    I2C_RW_Control(pBIOSInfo, I2C_WRITE_SCL, flag);
    if (flag) delayIn_usec(I2C_WAIT_TIME);
    delayIn_usec(I2C_WAIT_TIME);
}

/* Set SDA */
static void HWGPIOI2C_SetSDA(VIABIOSInfoPtr pBIOSInfo, CARD8 flag)
{
    I2C_RW_Control(pBIOSInfo, I2C_WRITE_SDA, flag);
    delayIn_usec(I2C_WAIT_TIME);
}

/* Get SDA */
static CARD8  HWGPIOI2C_GetSDA(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    CARD8   value;

    VGAOUT8(0x3c4, GPIOPORT);
    value = VGAIN8(0x3c5);
    if (value & I2C_INPUT_DATA)
        return 1;
    else
        return 0;
}

/* START Condition */
static void GPIOI2C_START(VIABIOSInfoPtr pBIOSInfo)
{
    HWGPIOI2C_SetSDA(pBIOSInfo, 1);
    HWGPIOI2C_SetSCL(pBIOSInfo, 1);
    delayIn_usec(STARTTIMEOUT);
    HWGPIOI2C_SetSDA(pBIOSInfo, 0);
    HWGPIOI2C_SetSCL(pBIOSInfo, 0);
}

/* STOP Condition */
static void GPIOI2C_STOP(VIABIOSInfoPtr pBIOSInfo)
{
    HWGPIOI2C_SetSDA(pBIOSInfo, 0);
    HWGPIOI2C_SetSCL(pBIOSInfo, 1);
    HWGPIOI2C_SetSDA(pBIOSInfo, 1);
    I2C_RW_Control(pBIOSInfo, I2C_RELEASE, 0);
    /* to make the differentiation of next START condition */
    delayIn_usec(I2C_WAIT_TIME);
}

/* Check ACK */
static Bool GPIOI2C_ACKNOWLEDGE(VIABIOSInfoPtr pBIOSInfo)
{
    CARD8   Status;

    I2C_RW_Control(pBIOSInfo, I2C_READ_SDA, 0);
    delayIn_usec(I2C_WAIT_TIME);
    HWGPIOI2C_SetSCL(pBIOSInfo, 1);
    Status = HWGPIOI2C_GetSDA(pBIOSInfo);
    I2C_RW_Control(pBIOSInfo, I2C_WRITE_SDA, Status);
    HWGPIOI2C_SetSCL(pBIOSInfo, 0);
    delayIn_usec(I2C_WAIT_TIME);

    if(Status) return FALSE;
    else return TRUE;
}

/* Send ACK */
static Bool GPIOI2C_SENDACKNOWLEDGE(VIABIOSInfoPtr pBIOSInfo)
{
    HWGPIOI2C_SetSDA(pBIOSInfo, 0);
    HWGPIOI2C_SetSCL(pBIOSInfo, 1);
    HWGPIOI2C_SetSCL(pBIOSInfo, 0);
    delayIn_usec(I2C_WAIT_TIME);

    return TRUE;
}

/* Send NACK */
static Bool GPIOI2C_SENDNACKNOWLEDGE(VIABIOSInfoPtr pBIOSInfo)
{
    HWGPIOI2C_SetSDA(pBIOSInfo, 1);
    HWGPIOI2C_SetSCL(pBIOSInfo, 1);
    HWGPIOI2C_SetSCL(pBIOSInfo, 0);
    delayIn_usec(I2C_WAIT_TIME);

    return TRUE;
}

/* Write Data(Bit by Bit) to I2C */
static Bool GPIOI2C_WriteData(VIABIOSInfoPtr pBIOSInfo, CARD8 Data)
{
    int     i;

    if (!GPIOI2C_WriteBit(pBIOSInfo, (Data >> 7) & 1, BYTETIMEOUT))
	    return FALSE;

    for (i = 6; i >= 0; i--)
    if (!GPIOI2C_WriteBit(pBIOSInfo, (Data >> i) & 1, BITTIMEOUT))
	    return FALSE;

    return GPIOI2C_ACKNOWLEDGE(pBIOSInfo);
}

static Bool GPIOI2C_WriteBit(VIABIOSInfoPtr pBIOSInfo, int sda, int timeout)
{
    Bool ret = TRUE;

    HWGPIOI2C_SetSDA(pBIOSInfo, sda);
    delayIn_usec(I2C_WAIT_TIME/5);
    HWGPIOI2C_SetSCL(pBIOSInfo, 1);
    delayIn_usec(HOLDTIME);
    delayIn_usec(timeout);
    HWGPIOI2C_SetSCL(pBIOSInfo, 0);
    delayIn_usec(I2C_WAIT_TIME/5);

    return ret;
}

static Bool GPIOI2C_ReadBit(VIABIOSInfoPtr pBIOSInfo, int *psda, int timeout)
{
    Bool ret = TRUE;

    I2C_RW_Control(pBIOSInfo, I2C_READ_SDA, 0);
    delayIn_usec(I2C_WAIT_TIME/5);
    HWGPIOI2C_SetSCL(pBIOSInfo, 1);
    delayIn_usec(HOLDTIME);
    delayIn_usec(timeout);
    *psda = HWGPIOI2C_GetSDA(pBIOSInfo);
    HWGPIOI2C_SetSCL(pBIOSInfo, 0);
    delayIn_usec(I2C_WAIT_TIME/5);

    return ret;
}

/* Read Data(Bit by Bit) from I2C */
static CARD8 GPIOI2C_ReadData(VIABIOSInfoPtr pBIOSInfo)
{
    int     i, sda;
    CARD8   data;

    if(!GPIOI2C_ReadBit(pBIOSInfo, &sda, BYTETIMEOUT))
	return 0;

    data = (sda > 0) << 7;
    for (i = 6; i >= 0; i--)
	if (!GPIOI2C_ReadBit(pBIOSInfo, &sda, BITTIMEOUT))
	    return 0;
	else
	    data |= (sda > 0) << i;

    return  data;
}

/* Write Data(one Byte) to Desired Device on I2C */
Bool VIAGPIOI2C_Write(VIABIOSInfoPtr pBIOSInfo, int SubAddress, CARD8 Data)
{
    int     Retry;
    Bool    Done = FALSE;

    for(Retry = 1; Retry <= WRITE_MAX_RETRIES; Retry++)
    {
        GPIOI2C_START(pBIOSInfo);

        if(!GPIOI2C_WriteData(pBIOSInfo, SLAVEADDR)) {

            GPIOI2C_STOP(pBIOSInfo);
            continue;
        }

        if(SubAddress & 0xF00) {            /* write 12-bit sub address */
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)((SubAddress/0x100)&0x0F))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)(SubAddress%0x100))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
        } else {                            /* write 8-bit sub address */
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)(SubAddress))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
        }

        if(!GPIOI2C_WriteData(pBIOSInfo, Data)) {

            GPIOI2C_STOP(pBIOSInfo);
            continue;
        }
        Done = TRUE;
        break;
    }

    GPIOI2C_STOP(pBIOSInfo);

    return Done;
}

/* Read Data from Desired Device on I2C */
Bool VIAGPIOI2C_Read(VIABIOSInfoPtr pBIOSInfo, int SubAddress, CARD8 *Buffer, int BufferLen)
{
    int     i, Retry;

    for(Retry = 1; Retry <= READ_MAX_RETRIES; Retry++)
    {
        GPIOI2C_START(pBIOSInfo);
        if(!GPIOI2C_WriteData(pBIOSInfo, SLAVEADDR&0xFE)) {

            GPIOI2C_STOP(pBIOSInfo);
            continue;
        }

        if(SubAddress & 0xF00) {        /* write 12-bit sub address */
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)((SubAddress/0x100)&0x0F))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)(SubAddress%0x100))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
        } else {                        /* write 8-bit sub address */
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)(SubAddress))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
        }

        break;
    }

    if (Retry > READ_MAX_RETRIES) return FALSE;

    for(Retry = 1; Retry <= READ_MAX_RETRIES; Retry++)
    {
        GPIOI2C_START(pBIOSInfo);
        if(!GPIOI2C_WriteData(pBIOSInfo, SLAVEADDR | 0x01)) {

            GPIOI2C_STOP(pBIOSInfo);
            continue;
        }
        for(i = 0; i < BufferLen; i++) {

            *Buffer = GPIOI2C_ReadData(pBIOSInfo);
            Buffer ++;
            if(BufferLen == 1)
                /*GPIOI2C_SENDACKNOWLEDGE(pBIOSInfo);*/ /* send ACK for normal operation */
                GPIOI2C_SENDNACKNOWLEDGE(pBIOSInfo);    /* send NACK for VT3191/VT3192 only */
            else if(i < BufferLen - 1)
                GPIOI2C_SENDACKNOWLEDGE(pBIOSInfo);     /* send ACK */
            else
                GPIOI2C_SENDNACKNOWLEDGE(pBIOSInfo);    /* send NACK */
        }
        GPIOI2C_STOP(pBIOSInfo);
        break;
    }

    if (Retry > READ_MAX_RETRIES)
        return FALSE;
    else
        return TRUE;
}

/* Read Data(one Byte) from Desired Device on I2C */
Bool VIAGPIOI2C_ReadByte(VIABIOSInfoPtr pBIOSInfo, int SubAddress, CARD8 *Buffer)
{
    int     Retry;

    for(Retry = 1; Retry <= READ_MAX_RETRIES; Retry++)
    {
        GPIOI2C_START(pBIOSInfo);
        if(!GPIOI2C_WriteData(pBIOSInfo, SLAVEADDR & 0xFE)) {

            GPIOI2C_STOP(pBIOSInfo);
            continue;
        }

        if(SubAddress & 0xF00) {        /* write 12-bit sub address */
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)((SubAddress/0x100)&0x0F))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)(SubAddress%0x100))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
        } else {                        /* write 8-bit sub address */
            if(!GPIOI2C_WriteData(pBIOSInfo, (CARD8)(SubAddress))) {

                GPIOI2C_STOP(pBIOSInfo);
                continue;
            }
        }

        break;
    }

    if (Retry > READ_MAX_RETRIES) return FALSE;

    for(Retry = 1; Retry <= READ_MAX_RETRIES; Retry++) {

        GPIOI2C_START(pBIOSInfo);

        if(!GPIOI2C_WriteData(pBIOSInfo, SLAVEADDR | 0x01)) {

            GPIOI2C_STOP(pBIOSInfo);
            continue;
        }

        *Buffer = GPIOI2C_ReadData(pBIOSInfo);

        GPIOI2C_STOP(pBIOSInfo);
        break;
    }

    if (Retry > READ_MAX_RETRIES)
        return FALSE;
    else
        return TRUE;
}
