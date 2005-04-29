/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimach64i2c.c,v 1.2 2004/01/05 16:42:02 tsi Exp $ */
/*
 * Copyright 2003 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "ati.h"
#include "atichip.h"
#include "atii2c.h"
#include "atimach64i2c.h"
#include "atimach64io.h"
#include "atituner.h"

/* MPP_CONFIG register values */
#define MPP_INIT     pATI->NewHW.mpp_config

#define MPP_WRITE    (MPP_INIT                                               )
#define MPP_WRITEINC (MPP_INIT | (MPP_AUTO_INC_EN                           ))
#define MPP_READ     (MPP_INIT | (                  MPP_BUFFER_MODE_PREFETCH))
#define MPP_READINC  (MPP_INIT | (MPP_AUTO_INC_EN | MPP_BUFFER_MODE_PREFETCH))

/*
 * ATIMach64MPPWaitForIdle --
 *
 * Support function to wait for the Multimedia Peripheral Port to become idle.
 * Currently, this function's return value indicates whether or not the port
 * became idle within 512 polling iterations.  For now, this value is ignored
 * by the rest of the code, but might be used in the future.
 */
static Bool
ATIMach64MPPWaitForIdle
(
    ATIPtr pATI
)
{
    CARD32 Count = 0x0200;

    while (in8(MPP_CONFIG + 3) & GetByte(MPP_BUSY, 3))
    {
        if (!--Count)
            return FALSE;
        usleep(1);              /* XXX Excessive? */
    }

    return TRUE;
}

/*
 * ATIMach64MPPSetAddress --
 *
 * Sets a 16-bit ImpacTV address on the Multimedia Peripheral Port.
 */
static void
ATIMach64MPPSetAddress
(
    ATIPtr pATI,
    CARD16 Address
)
{
    ATIMach64MPPWaitForIdle(pATI);
    outr(MPP_CONFIG, MPP_WRITEINC);
    outr(MPP_ADDR, 0x00000008U);
    out8(MPP_DATA, (CARD8)Address);
    ATIMach64MPPWaitForIdle(pATI);
    out8(MPP_DATA, (CARD8)(Address >> 8));
    ATIMach64MPPWaitForIdle(pATI);
    outr(MPP_CONFIG, MPP_WRITE);
    outr(MPP_ADDR, 0x00000018U);
    ATIMach64MPPWaitForIdle(pATI);
}

/*
 * ATIMach64ImpacTVProbe --
 *
 * This probes for an ImpacTV chip and returns its chip ID, or 0.
 */
static int
ATIMach64ImpacTVProbe
(
    int    iScreen,
    ATIPtr pATI
)
{
    CARD8 ChipID = 0;

    /* Assume ATIModePreInit() has already been called */
    outr(MPP_STROBE_SEQ, pATI->NewHW.mpp_strobe_seq);
    outr(TVO_CNTL, pATI->NewHW.tvo_cntl);

    outr(MPP_CONFIG, MPP_READ);
    ATIMach64MPPWaitForIdle(pATI);
    outr(MPP_ADDR, 0x0000000AU);
    if (!(ChipID = in8(MPP_DATA)))
    {
         ATIMach64MPPWaitForIdle(pATI);
         outr(MPP_ADDR, 0x00000023U);
         if ((ChipID = in8(MPP_DATA)) != 0x54U)
         {
             ATIMach64MPPWaitForIdle(pATI);
             outr(MPP_ADDR, 0x0000000BU);
             ChipID = in8(MPP_DATA);
         }
    }
    ATIMach64MPPWaitForIdle(pATI);
    outr(MPP_CONFIG, MPP_WRITE);

    if (ChipID)
        xf86DrvMsg(iScreen, X_PROBED, "ImpacTV chip ID 0x%02X detected.\n",
            ChipID);

    return (int)(CARD16)ChipID;
}

/*
 * ATIMach64ImpacTVSetBits --
 *
 * Controls I2C SDA and SCL lines through ImpacTV.
 */
static void
ATIMach64ImpacTVSetBits
(
    ATII2CPtr pATII2C,
    ATIPtr    pATI,
    CARD32    Bits
)
{
    pATII2C->I2CCur = Bits;

    ATIMach64MPPSetAddress(pATI, IT_I2C_CNTL);

    outr(MPP_CONFIG, MPP_WRITE);

    out8(MPP_DATA, (CARD8)Bits);

    ATIMach64MPPWaitForIdle(pATI);
}

/*
 * ATIMach64ImpacTVGetBits --
 *
 * Returns the status of an ImpacTV's I2C control lines.
 */
static CARD32
ATIMach64ImpacTVGetBits
(
    ATIPtr    pATI
)
{
    ATIMach64MPPSetAddress(pATI, IT_I2C_CNTL);

    outr(MPP_CONFIG, MPP_READ);

    ATIMach64MPPWaitForIdle(pATI);

    return in8(MPP_DATA);
}

/*
 * ATIMach64I2C_CNTLSetBits --
 *
 * Controls SDA and SCL lines through a 3D Rage Pro's hardware assisted I2C.
 */
static void
ATIMach64I2C_CNTLSetBits
(
    ATII2CPtr pATII2C,
    ATIPtr    pATI,
    CARD32    Bits
)
{
    pATII2C->I2CCur = Bits;

    out8(I2C_CNTL_0 + 1, (CARD8)Bits);
}

/*
 * ATIMach64I2C_CNTLGetBits --
 *
 * Returns the status of a 3D Rage Pro's hardware assisted I2C control lines.
 */
static CARD32
ATIMach64I2C_CNTLGetBits
(
    ATIPtr    pATI
)
{
    return in8(I2C_CNTL_0 + 1);
}

/*
 * ATIMach64GP_IOSetBits --
 *
 * Controls SDA and SCL control lines through a Mach64's GP_IO register.
 */
static void
ATIMach64GP_IOSetBits
(
    ATII2CPtr pATII2C,
    ATIPtr    pATI,
    CARD32    Bits
)
{
    pATII2C->I2CCur = Bits;

    outr(GP_IO, Bits);
}

/*
 * ATIMach64GP_IOGetBits --
 *
 * Returns the status of I2C control lines through a Mach64's GP_IO register.
 */
static CARD32
ATIMach64GP_IOGetBits
(
    ATIPtr    pATI
)
{
    return inr(GP_IO);
}

#define GPIO1_MASK \
    (DAC_GIO_STATE_1 | DAC_GIO_DIR_1)
#define GPIO2_MASK \
    (GEN_GIO2_DATA_OUT | GEN_GIO2_DATA_IN | GEN_GIO2_WRITE)

/*
 * ATIMach64DAC_GENSetBits --
 *
 * Controls SDA and SCL control lines through a Mach64's GEN_TEST_CNTL and
 * DAC_CNTL registers.
 */
static void
ATIMach64DAC_GENSetBits
(
    ATII2CPtr pATII2C,
    ATIPtr    pATI,
    CARD32    Bits
)
{
    CARD32 tmp;

    pATII2C->I2CCur = Bits;

    tmp = inr(DAC_CNTL) & ~GPIO1_MASK;
    outr(DAC_CNTL, tmp | (Bits & GPIO1_MASK));
    tmp = inr(GEN_TEST_CNTL) & ~GPIO2_MASK;
    outr(GEN_TEST_CNTL, tmp | (Bits & GPIO2_MASK));
}

/*
 * ATIMach64DAC_GENGetBits --
 *
 * Returns the status of I2C control lines through a Mach64's GEN_TEST_CNTL and
 * DAC_CNTL registers.
 */
static CARD32
ATIMach64DAC_GENGetBits
(
    ATIPtr    pATI
)
{
    return (inr(DAC_CNTL) & GPIO1_MASK) | (inr(GEN_TEST_CNTL) & GPIO2_MASK);
}

/*
 * ATITVAddOnProbe --
 *
 * Probe for an ATI-TV add-on card at specific addresses on an I2C bus.
 */
static Bool
ATITVAddOnProbe
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    I2CBusPtr   pI2CBus
)
{
    I2CDevPtr pI2CDev = xnfcalloc(1, SizeOf(I2CDevRec));
    int       Index;
    I2CByte   tmp;

    static const CARD8 ATITVAddOnAddresses[] = {0x70, 0x40, 0x78, 0x72, 0x42};

    pI2CDev->DevName      = "ATI-TV Add-on";
    pI2CDev->pI2CBus      = pI2CBus;
    pI2CDev->StartTimeout = pI2CBus->StartTimeout;
    pI2CDev->BitTimeout   = pI2CBus->BitTimeout;
    pI2CDev->AcknTimeout  = pI2CBus->AcknTimeout;
    pI2CDev->ByteTimeout  = pI2CBus->ByteTimeout;

    for (Index = 0;  Index < NumberOf(ATITVAddOnAddresses);  Index++)
    {
        pI2CDev->SlaveAddr = ATITVAddOnAddresses[Index];

        if (xf86I2CFindDev(pI2CBus, pI2CDev->SlaveAddr))
            continue;

        tmp = 0xFFU;

        if (!(*pI2CBus->I2CWriteRead)(pI2CDev, &tmp, 1, NULL, 0) ||
            !(*pI2CBus->I2CWriteRead)(pI2CDev, NULL, 0, &tmp, 1) ||
            (tmp == 0xFFU) || ((tmp = tmp & 0x1FU) == /*ATI_TUNER_NONE*/0))
            continue;

        if (!xf86I2CDevInit(pI2CDev))
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Failed to register I2C device for ATI-TV add-on.\n");
            break;
        }

        if (pATI->Tuner != tmp)
        {
            if (pATI->Tuner != ATI_TUNER_NONE)
                xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                    "Tuner type mismatch:  BIOS 0x%x, ATI-TV 0x%x.\n",
                    pATI->Tuner, tmp);

            pATI->Tuner = tmp;
        }

        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
            "%s tuner detected on ATI-TV add-on adapter at I2C bus address"
            " 0x%2x.\n", ATITuners[pATI->Tuner].name, pI2CDev->SlaveAddr);

        return TRUE;
    }

    xfree(pI2CDev);
    return FALSE;
}

/*
 * ATIMach64I2CPreInit --
 *
 * This function potentially allocates an I2CBusRec and initialises it with
 * ATI-specific and Mach64-specific information.
 */
void
ATIMach64I2CPreInit
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    I2CBusPtr pI2CBus;
    ATII2CPtr pATII2C;

    if ((pATI->Chip < ATI_CHIP_264CT) || (pATI->Chip >= ATI_CHIP_Mach64))
        return;

    /* Create an I2CBusRec and generically prime it */
    if (!(pI2CBus = ATICreateI2CBusRec(pScreenInfo->scrnIndex, pATI, "Mach64")))
        return;

    pATII2C = pI2CBus->DriverPrivate.ptr;

    switch (pATI->Chip)
    {
        case ATI_CHIP_264GTPRO:
        case ATI_CHIP_264LTPRO:
        case ATI_CHIP_264XL:
        case ATI_CHIP_MOBILITY:
            /*
             * These have I2C-specific registers.  Assume older I2C access
             * mechanisms are inoperative.
             */
            pATII2C->I2CSetBits = ATIMach64I2C_CNTLSetBits;
            pATII2C->I2CGetBits = ATIMach64I2C_CNTLGetBits;
            pATII2C->SCLDir = pATII2C->SDADir = 0;
            pATII2C->SCLGet = pATII2C->SCLSet = GetByte(I2C_CNTL_SCL, 1);
            pATII2C->SDAGet = pATII2C->SDASet = GetByte(I2C_CNTL_SDA, 1);

            out8(I2C_CNTL_1 + 2, GetByte(I2C_SEL, 2));
            out8(I2C_CNTL_0 + 0,
                GetByte(I2C_CNTL_STAT | I2C_CNTL_HPTR_RST, 0));
            break;

        case ATI_CHIP_264VTB:
        case ATI_CHIP_264GTB:
        case ATI_CHIP_264VT3:
        case ATI_CHIP_264GTDVD:
        case ATI_CHIP_264LT:
        case ATI_CHIP_264VT4:
        case ATI_CHIP_264GT2C:
            /* If an ImpacTV chip is found, use it to provide I2C access */
            if (ATIMach64ImpacTVProbe(pScreenInfo->scrnIndex, pATI))
            {
                pATII2C->I2CSetBits = ATIMach64ImpacTVSetBits;
                pATII2C->I2CGetBits = ATIMach64ImpacTVGetBits;
                pATII2C->SCLDir = IT_SCL_DIR;
                pATII2C->SCLGet = IT_SCL_GET;
                pATII2C->SCLSet = IT_SCL_SET;
                pATII2C->SDADir = IT_SDA_DIR;
                pATII2C->SDAGet = IT_SDA_GET;
                pATII2C->SDASet = IT_SDA_SET;

                ATIMach64MPPSetAddress(pATI, IT_I2C_CNTL);
                outr(MPP_CONFIG, MPP_WRITEINC);
                out8(MPP_DATA, 0x00U);
                out8(MPP_DATA, 0x55U);
                out8(MPP_DATA, 0x00U);
                out8(MPP_DATA, 0x00U);
                ATIMach64MPPWaitForIdle(pATI);
                break;
            }
            /* Otherwise, fall through to the older case */

        case ATI_CHIP_264VT:
        case ATI_CHIP_264GT:
            /* First try GIO pins 11 (clock) and 4 (data) */
            pATII2C->I2CSetBits = ATIMach64GP_IOSetBits;
            pATII2C->I2CGetBits = ATIMach64GP_IOGetBits;
            pATII2C->SCLDir = GP_IO_DIR_B;
            pATII2C->SCLGet = pATII2C->SCLSet = GP_IO_B;
            pATII2C->SDADir = GP_IO_DIR_4;
            pATII2C->SDAGet = pATII2C->SDASet = GP_IO_4;

            if (ATITVAddOnProbe(pScreenInfo, pATI, pI2CBus))
                break;

            /* Next, try pins 10 (clock) and 12 (data) */
            pATII2C->SCLDir = GP_IO_DIR_A;
            pATII2C->SCLGet = pATII2C->SCLSet = GP_IO_A;
            pATII2C->SDADir = GP_IO_DIR_C;
            pATII2C->SDAGet = pATII2C->SDASet = GP_IO_C;

            if (ATITVAddOnProbe(pScreenInfo, pATI, pI2CBus))
                break;
            /* Otherwise, fall back to ATI's first I2C implementation */

        default:
            /*
             * First generation integrated controllers access GIO pin 1 (clock)
             * though DAC_CNTL, and pin 2 (data) through GEN_TEST_CNTL.
             */
            pATII2C->I2CSetBits = ATIMach64DAC_GENSetBits;
            pATII2C->I2CGetBits = ATIMach64DAC_GENGetBits;
            pATII2C->SCLDir = DAC_GIO_DIR_1;
            pATII2C->SCLGet = pATII2C->SCLSet = DAC_GIO_STATE_1;
            pATII2C->SDADir = GEN_GIO2_WRITE;
            pATII2C->SDAGet = GEN_GIO2_DATA_IN;
            pATII2C->SDASet = GEN_GIO2_DATA_OUT;

            (void)ATITVAddOnProbe(pScreenInfo, pATI, pI2CBus);
            break;
    }
}
