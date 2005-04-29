/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atii2c.c,v 1.4 2004/01/05 16:42:02 tsi Exp $ */
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

#include "atiadapter.h"
#include "atii2c.h"
#include "atiload.h"
#include "atimach64i2c.h"
#include "atistruct.h"

#include "xf86.h"

/* This is derived from GATOS code, with a liberal sprinkling of bug fixes */

/*
 * Some local macros for use by the mid-level I2C functions.
 */

#define ATII2CDelay                                            \
    (*pI2CBus->I2CUDelay)(pI2CBus, pI2CBus->HoldTime)


#define ATII2CSCLDirOff                                        \
    if (pATII2C->SCLDir != 0)                                  \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur & ~pATII2C->SCLDir)

#define ATII2CSCLDirOn                                         \
    if (pATII2C->SCLDir != 0)                                  \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur | pATII2C->SCLDir)

#define ATII2CSDADirOff                                        \
    if (pATII2C->SDADir != 0)                                  \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur & ~pATII2C->SDADir)

#define ATII2CSDADirOn                                         \
    if (pATII2C->SDADir != 0)                                  \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur | pATII2C->SDADir)


#define ATII2CSCLBitGet                                        \
    ((*pATII2C->I2CGetBits)(pATI) & pATII2C->SCLGet)

#define ATII2CSCLBitOff                                        \
    do                                                         \
    {                                                          \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur & ~pATII2C->SCLSet);               \
        ATII2CDelay;                                           \
    } while (0)

#define ATII2CSCLBitOn                                         \
    do                                                         \
    {                                                          \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur | pATII2C->SCLSet);                \
        do      /* Wait until all devices have released SCL */ \
        {                                                      \
            ATII2CDelay;                                       \
        } while (ATII2CSCLBitGet == 0);                        \
    } while (0)


#define ATII2CSDABitGet                                        \
    ((*pATII2C->I2CGetBits)(pATI) & pATII2C->SDAGet)

#define ATII2CSDABitOff                                        \
    do                                                         \
    {                                                          \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur & ~pATII2C->SDASet);               \
        ATII2CDelay;                                           \
    } while (0)

#define ATII2CSDABitOn                                         \
    do                                                         \
    {                                                          \
        (*pATII2C->I2CSetBits)(pATII2C, pATI,                  \
            pATII2C->I2CCur | pATII2C->SDASet);                \
        ATII2CDelay;                                           \
    } while (0)

#define ATII2CSDABitSet(_flag)                                 \
    do                                                         \
    {                                                          \
        if (_flag)                                             \
            ATII2CSDABitOn;                                    \
        else                                                   \
            ATII2CSDABitOff;                                   \
    } while (0)


/*
 * ATII2CAddress --
 *
 * This function puts a Start bit and an 8-bit address on the I2C bus.
 */
static Bool
ATII2CAddress
(
    I2CDevPtr    pI2CDev,
    I2CSlaveAddr Address
)
{
    I2CBusPtr pI2CBus = pI2CDev->pI2CBus;
    ATII2CPtr pATII2C = pI2CBus->DriverPrivate.ptr;
    ATIPtr    pATI    = pATII2C->pATI;

    /*
     * Set I2C line directions to out-bound.  SCL will remain out-bound until
     * next I2C Stop.
     */
    ATII2CSCLDirOn;
    ATII2CSDADirOn;

    /*
     * Send Start bit.  This is a pull-down of the data line while the clock
     * line is pulled up.
     */
    ATII2CSDABitOn;
    ATII2CSCLBitOn;
    ATII2CSDABitOff;
    ATII2CSCLBitOff;

    /* Send low byte of device address */
    if ((*pI2CBus->I2CPutByte)(pI2CDev, (I2CByte)Address))
    {
        /* Send top byte of address, if appropriate */
        if (((Address & 0x00F8U) != 0x00F0U) &&
            ((Address & 0x00FEU) != 0x0000U))
            return TRUE;

        if ((*pI2CBus->I2CPutByte)(pI2CDev, (I2CByte)(Address >> 8)))
            return TRUE;
    }

    /* Kill I2C transaction on failure */
    (*pI2CBus->I2CStop)(pI2CDev);
    return FALSE;
}

/*
 * ATII2CStop --
 *
 * This function puts a stop signal on the I2C bus.
 */
static void
ATII2CStop
(
    I2CDevPtr pI2CDev
)
{
    I2CBusPtr pI2CBus = pI2CDev->pI2CBus;
    ATII2CPtr pATII2C = pI2CBus->DriverPrivate.ptr;
    ATIPtr    pATI    = pATII2C->pATI;

    ATII2CSDADirOn;             /* Set data line direction to out-bound */

    /*
     * Send Stop bit.  This is a pull-up of the data line while the clock line
     * is pulled up.
     */
    ATII2CSDABitOff;
    ATII2CSCLBitOn;
    ATII2CSDABitOn;
    ATII2CSCLBitOff;

    /* Reset I2C line directions to in-bound */
    ATII2CSCLDirOff;
    ATII2CSDADirOff;
}

/*
 * ATII2CPutByte --
 *
 * This function puts an 8-bit value on the I2C bus, starting with its MSB.
 */
static Bool
ATII2CPutByte
(
    I2CDevPtr pI2CDev,
    I2CByte   Data
)
{
    I2CBusPtr pI2CBus = pI2CDev->pI2CBus;
    ATII2CPtr pATII2C = pI2CBus->DriverPrivate.ptr;
    ATIPtr    pATI    = pATII2C->pATI;
    int       i;
    Bool      Result;

    ATII2CSDADirOn;             /* Set data line direction to out-bound */

    /* Send data byte */
    for (i = 0;  i < 8;  i++)
    {
        ATII2CSDABitSet(Data & 0x80U);
        ATII2CSCLBitOn;
        ATII2CSCLBitOff;

        Data <<= 1;
    }

    ATII2CSDABitOn;             /* Release data line */

    ATII2CSDADirOff;            /* Set data line direction to in-bound */

    ATII2CSCLBitOn;             /* Start bit-read clock pulse */

    /* Get [N]ACK bit */
    if (ATII2CSDABitGet)
        Result = FALSE;
    else
        Result = TRUE;

    ATII2CSCLBitOff;            /* End clock pulse */

    return Result;
}

/*
 * ATII2CGetByte --
 *
 * This function retrieves an 8-bit value from the I2C bus.
 */
static Bool
ATII2CGetByte
(
    I2CDevPtr pI2CDev,
    I2CByte   *pData,
    Bool      Last
)
{
    I2CBusPtr     pI2CBus = pI2CDev->pI2CBus;
    ATII2CPtr     pATII2C = pI2CBus->DriverPrivate.ptr;
    ATIPtr        pATI    = pATII2C->pATI;
    unsigned long Value   = 1;

    do
    {
        ATII2CSCLBitOn;         /* Start bit-read clock pulse */

        /* Accumulate bit into byte value */
        Value <<= 1;
        if (ATII2CSDABitGet)
            Value++;

        ATII2CSCLBitOff;        /* End clock pulse */
    } while (Value <= (unsigned long)((I2CByte)(-1)));

    *pData = (I2CByte)Value;

    ATII2CSDADirOn;             /* Set data line direction to out-bound */

    /* Send [N]ACK bit */
    ATII2CSDABitSet(Last);
    ATII2CSCLBitOn;
    ATII2CSCLBitOff;

    if (!Last)
        ATII2CSDABitOn;         /* Release data line */

    ATII2CSDADirOff;            /* Set data line direction to in-bound */

    return TRUE;
}

/*
 * ATICreateI2CBusRec --
 *
 * This function is called to initialise an I2CBusRec.
 */
I2CBusPtr
ATICreateI2CBusRec
(
    int    iScreen,
    ATIPtr pATI,
    char   *BusName
)
{
    I2CBusPtr pI2CBus;
    ATII2CPtr pATII2C = xnfcalloc(1, SizeOf(ATII2CRec));

    if (!(pI2CBus = xf86CreateI2CBusRec()))
    {
        xf86DrvMsg(iScreen, X_WARNING, "Unable to allocate I2C Bus record.\n");
        xfree(pATII2C);
        return NULL;
    }

    /* Fill in generic structure fields */
    pI2CBus->BusName           = BusName;
    pI2CBus->scrnIndex         = iScreen;

    pI2CBus->I2CAddress        = ATII2CAddress;
    pI2CBus->I2CStop           = ATII2CStop;
    pI2CBus->I2CPutByte        = ATII2CPutByte;
    pI2CBus->I2CGetByte        = ATII2CGetByte;

    pI2CBus->DriverPrivate.ptr = pATII2C;

    pATII2C->pATI              = pATI;

    if (xf86I2CBusInit(pI2CBus))
        return pI2CBus;

    xf86DrvMsg(iScreen, X_WARNING,
        "I2C bus %s initialisation failure.\n", BusName);
    xf86DestroyI2CBusRec(pI2CBus, TRUE, TRUE);
    xfree(pATII2C);
    return NULL;
}

/*
 * ATII2CPreInit --
 *
 * This is called by ATIPreInit() to create I2C bus record(s) for the adapter.
 */
void
ATII2CPreInit
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    switch (pATI->Adapter)
    {
        case ATI_ADAPTER_MACH64:
            if (!ATILoadModule(pScreenInfo, "i2c", ATIi2cSymbols))
                return;

            ATIMach64I2CPreInit(pScreenInfo, pATI);
            break;

        default:
            break;
    }
}

/*
 * ATII2CFreeScreen --
 *
 * This is called by ATIFreeScreen() to remove the driver's I2C interface.
 */
void
ATII2CFreeScreen
(
    int iScreen
)
{
    I2CBusPtr pI2CBus, *ppI2CBus;
    ATII2CPtr pATII2C;
    int nI2CBus;

    nI2CBus = xf86I2CGetScreenBuses(iScreen, &ppI2CBus);
    while (--nI2CBus >= 0)
    {
        pI2CBus = ppI2CBus[nI2CBus];
        pATII2C = pI2CBus->DriverPrivate.ptr;

        xf86DestroyI2CBusRec(pI2CBus, TRUE, TRUE);
        xfree(pATII2C);
    }

    xfree(ppI2CBus);
}
