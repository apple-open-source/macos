/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_i2c.c,v 1.4 2003/12/31 05:42:05 dawes Exp $ */
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


#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "via_driver.h"

/*
 * DDC2 support requires DDC_SDA_MASK and DDC_SCL_MASK
 */
#define DDC_SDA_READ_MASK  (1 << 2)
#define DDC_SCL_READ_MASK  (1 << 3)
#define DDC_SDA_WRITE_MASK (1 << 4)
#define DDC_SCL_WRITE_MASK (1 << 5)

/* I2C Function for DDC2 */
static void
VIAI2C1PutBits(I2CBusPtr b, int clock,  int data)
{
    CARD8 reg;

    outb(0x3c4, 0x26);
    reg = inb(0x3c5);
    reg &= 0xF0;
    reg |= 0x01;                    /* Enable DDC */

    if (clock)
        reg |= DDC_SCL_WRITE_MASK;
    else
        reg &= ~DDC_SCL_WRITE_MASK;

    if (data)
        reg |= DDC_SDA_WRITE_MASK;
    else
        reg &= ~DDC_SDA_WRITE_MASK;

    outb(0x3c4, 0x26);
    outb(0x3c5, reg);
}

static void
VIAI2C1GetBits(I2CBusPtr b, int *clock, int *data)
{
    CARD8 reg;

    outb(0x3c4, 0x26);
    reg = inb(0x3c5);

    *clock = (reg & DDC_SCL_READ_MASK) != 0;
    *data  = (reg & DDC_SDA_READ_MASK) != 0;
}

/* Function for DVI DDC2. Also used for the tuner and TV IC's */

static void
VIAI2C2PutBits(I2CBusPtr b, int clock,  int data)
{
    CARD8 reg;

    outb(0x3c4, 0x31);
    reg = inb(0x3c5);
    reg &= 0xF0;
    reg |= 0x01;                    /* Enable DDC */

    if (clock)
        reg |= DDC_SCL_WRITE_MASK;
    else
        reg &= ~DDC_SCL_WRITE_MASK;

    if (data)
        reg |= DDC_SDA_WRITE_MASK;
    else
        reg &= ~DDC_SDA_WRITE_MASK;

    outb(0x3c4, 0x31);
    outb(0x3c5, reg);
}

static void
VIAI2C2GetBits(I2CBusPtr b, int *clock, int *data)
{
    CARD8 reg;

    outb(0x3c4, 0x31);
    reg = inb(0x3c5);

    *clock = (reg & DDC_SCL_READ_MASK) != 0;
    *data  = (reg & DDC_SDA_READ_MASK) != 0;
}


Bool
VIAI2CInit(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    I2CBusPtr I2CPtr1, I2CPtr2;

    I2CPtr1 = xf86CreateI2CBusRec();
    I2CPtr2 = xf86CreateI2CBusRec();
    if (!I2CPtr1 || !I2CPtr2)
        return FALSE;

    I2CPtr1->BusName    = "I2C bus 1";
    I2CPtr1->scrnIndex  = pScrn->scrnIndex;
    I2CPtr1->I2CPutBits = VIAI2C1PutBits;
    I2CPtr1->I2CGetBits = VIAI2C1GetBits;

    I2CPtr2->BusName    = "I2C bus 2";
    I2CPtr2->scrnIndex  = pScrn->scrnIndex;
    I2CPtr2->I2CPutBits = VIAI2C2PutBits;
    I2CPtr2->I2CGetBits = VIAI2C2GetBits;

    if (!xf86I2CBusInit(I2CPtr1) || !xf86I2CBusInit(I2CPtr2))
        return FALSE;

    pVia->I2C_Port1 = I2CPtr1;
    pVia->I2C_Port2 = I2CPtr2;

    return TRUE;
}

#ifdef _MY_I2C_
/*------------------------------------------------
   I2C Module
  ------------------------------------------------*/

static int pcI2CIndex   = 0x3c4;
static int pcI2Cport    = 0x3c5;
/*
static int pcIndexReg   = 0x31;
static int pcSDAReadBack= 0x31;
*/

static int gSDA=0;


#if 0
static void I2CUDelay(int usec)
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
#endif

/* Enable I2C */
void I2C_Enable(int pcIndexReg)
{
    int tempI2Cdata, Reg3C4H;

    /* save 3c4H */
    Reg3C4H = inb(pcI2CIndex);

    outb(pcI2CIndex, pcIndexReg);
    tempI2Cdata = inb(pcI2Cport);

    tempI2Cdata |= 0x01; /* Bit 0:I2C Serial Port Enable */
    outb(pcI2Cport, tempI2Cdata);

    /* restore 3c4H */
    outb(pcI2CIndex, Reg3C4H);

} /* I2C_enable */



/* reverse data */
long I2C_reverk(register unsigned data)
{
    unsigned long rdata = 0;
    int i;

    for ( i = 0; i < 16 ; i++ ) {
        rdata |= ( data & 1 ); /* strip off LSBIT */
        data >>= 1;
        rdata <<= 1;
    }
    return(rdata >> 1);

} /* I2C_reverk */



/* get an acknowledge back from a slave device */
int I2C_ack_pc(int pcIndexReg)
{
    int ack;

    I2C_regwrit_pc(pcIndexReg, I2C_SDA, 1);
    I2C_regwrit_pc(pcIndexReg, I2C_SCL, HICLK);
    ack = I2C_regread_pc(pcIndexReg, I2C_SDA);
    I2C_regwrit_pc(pcIndexReg, I2C_SCL, LOCLK);

    return (ack);

}  /* I2C_ack_pc */



/* send a start condition */
void I2C_start_pc(int pcIndexReg)
{
    I2C_regwrit_pc(pcIndexReg, I2C_SDA, 1);
    I2C_regwrit_pc(pcIndexReg, I2C_SCL, HICLK);
    I2C_regwrit_pc(pcIndexReg, I2C_SDA, 0);
    I2C_regwrit_pc(pcIndexReg, I2C_SCL, LOCLK);

} /* I2C_start_pc */



/* send a stop condition */
void I2C_stop_pc(int pcIndexReg)
{
    I2C_regwrit_pc(pcIndexReg, I2C_SDA, 0);
    I2C_regwrit_pc(pcIndexReg, I2C_SCL, HICLK);
    I2C_regwrit_pc(pcIndexReg, I2C_SDA, 1);

} /* I2C_stop_pc */



/*  write I2C data */
int I2C_wdata_pc(int pcIndexReg, unsigned type , unsigned data)
{
    int i;

    data = (unsigned int)(I2C_reverk(data) >> 8);  /* MSBIT goes out first */

    if ( type == I2C_ADR )
        I2C_start_pc(pcIndexReg);

    for ( i = 0; i < 8; data >>=1, i++ ) {
        I2C_regwrit_pc(pcIndexReg, I2C_SDA, data);
        I2C_regwrit_pc(pcIndexReg, I2C_SCL, HICLK);
        I2C_regwrit_pc(pcIndexReg, I2C_SCL, LOCLK);
    }

    return I2C_ack_pc(pcIndexReg);  /* wait for acknowledge */

} /* I2C_wdata_pc */


/* Write SCL/SDA bit */
void I2C_regwrit_pc(int pcIndexReg, unsigned type, unsigned data )
{
    int tempI2Cdata, Reg3C4H;

    /* save 3c4H */
    Reg3C4H = inb(pcI2CIndex);

    outb(pcI2CIndex, pcIndexReg);
    tempI2Cdata = inb(pcI2Cport);


    switch (type) {
        case I2C_SCL:
            tempI2Cdata &= 0xcf;  /* bit5 SPCLCK, bit4 SDATA */
            tempI2Cdata |= gSDA | ( (data & 1)<< 5);
            outb(pcI2Cport, tempI2Cdata);
            break;

        case I2C_SDA:
            tempI2Cdata &= 0xef;
            tempI2Cdata |= ( (data & 1) << 4);
            outb(pcI2Cport, tempI2Cdata);

            gSDA = 0;
            gSDA = ( (data & 1) << 4);

            break;
    }

    /* restore 3c4H */
    outb(pcI2CIndex, Reg3C4H);

} /* I2C_regwrit_pc */



/* Read SDA bit */
int I2C_regread_pc(int pcIndexReg, unsigned type)
{
    int temp=0,Reg3C4H;

    /* save 3c4H */
    Reg3C4H = inb(pcI2CIndex);

    switch (type) {
        case I2C_SCL :
            break;

        case I2C_SDA:
            outb(pcI2CIndex, pcIndexReg);
            temp = ( inb(pcI2Cport) >> 2) & 0x01;
            break;
    }

    /* restore 3c4H */
    outb(pcI2CIndex, Reg3C4H);

    return(temp);

} /* I2C_regread_pc */


void  I2C_wdata(int pcIndexReg, int addr, int subAddr, int data)
{
     int ack = 1;

     ack = I2C_wdata_pc(pcIndexReg, I2C_ADR, addr);
     ack = I2C_wdata_pc(pcIndexReg, I2C_DAT, subAddr);
     ack = I2C_wdata_pc(pcIndexReg, I2C_DAT, data);

     I2C_stop_pc(pcIndexReg);
}


int  I2C_rdata(int pcIndexReg, int addr, unsigned subAddr)
{
    int StatusData =0, data, i;

    I2C_wdata_pc(pcIndexReg, I2C_ADR, addr);
    I2C_wdata_pc(pcIndexReg, I2C_DAT, subAddr);
    I2C_stop_pc(pcIndexReg);

    I2C_wdata_pc(pcIndexReg, I2C_ADR, addr+1);


    /*  pull SDA High */
    I2C_regwrit_pc(pcIndexReg, I2C_SDA, 1);

    /* Read Register */
    for ( i = 0; i <= 7 ; i++ ) {

        I2C_regwrit_pc(pcIndexReg, I2C_SCL, HICLK);
        data = I2C_regread_pc(pcIndexReg, I2C_SDA);
        I2C_regwrit_pc(pcIndexReg, I2C_SCL, LOCLK);

        data &=  0x01; /* Keep SDA only */
        StatusData <<=   1;
        StatusData |= data;
    }

    I2C_stop_pc(pcIndexReg);
    return(StatusData);
}

Bool I2C_Write(int pcIndexReg, int addr, unsigned char *WriteBuffer, int nWrite)
{
    int s = 0;
    int ack = 1;

    ack = I2C_wdata_pc(pcIndexReg, I2C_ADR, addr);

    if (nWrite > 0) {
        for (; nWrite > 0; WriteBuffer++, nWrite--)
            ack = I2C_wdata_pc(pcIndexReg, I2C_DAT, *WriteBuffer);
        s++;
    }
    else {
        I2C_stop_pc(pcIndexReg);
        return (s);
    }

    I2C_stop_pc(pcIndexReg);
    return (s);
}
#endif
