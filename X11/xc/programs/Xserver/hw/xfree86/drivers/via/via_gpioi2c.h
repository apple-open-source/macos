/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_gpioi2c.h,v 1.2 2003/08/27 15:16:09 tsi Exp $ */
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

#ifndef _VIA_GPIOI2C_H_
#define _VIA_GPIOI2C_H_ 1

#define GPIOPORT                (pBIOSInfo->GPIOI2CInfo.bGPIOPort)
#define SLAVEADDR               (pBIOSInfo->GPIOI2CInfo.bSlaveAddr)
#define I2C_WAIT_TIME           (pBIOSInfo->GPIOI2CInfo.I2C_WAIT_TIME)
#define STARTTIMEOUT            (pBIOSInfo->GPIOI2CInfo.STARTTIMEOUT)
#define BYTETIMEOUT             (pBIOSInfo->GPIOI2CInfo.BYTETIMEOUT)
#define HOLDTIME                (pBIOSInfo->GPIOI2CInfo.HOLDTIME)
#define BITTIMEOUT              (pBIOSInfo->GPIOI2CInfo.BITTIMEOUT)

#define GPIOI2C_MASKD           0xC0
#define GPIOI2C_SCL_MASK        0x80
#define GPIOI2C_SCL_WRITE       0x80
#define GPIOI2C_SCL_READ        0x80
#define GPIOI2C_SDA_MASK        0x40
#define GPIOI2C_SDA_WRITE       0x40
#define GPIOI2C_SDA_READ        0x00

#define I2C_RELEASE             0x00
#define I2C_WRITE_SCL           0x01
#define I2C_READ_SCL            0x02
#define I2C_WRITE_SDA           0x03
#define I2C_READ_SDA            0x04

#define I2C_SDA_SCL_MASK        0x30
#define I2C_SDA_SCL             0x30
#define I2C_OUTPUT_CLOCK        0x20
#define I2C_OUTPUT_DATA         0x10
#define I2C_INPUT_CLOCK         0x08
#define I2C_INPUT_DATA          0x04

#define READ_MAX_RETRIES        20
#define WRITE_MAX_RETRIES       20

#endif /* _VIA_GPIOI2C_H_ */
