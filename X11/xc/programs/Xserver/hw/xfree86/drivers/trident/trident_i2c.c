/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_i2c.c,v 1.5 1999/10/13 20:02:30 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "trident.h"
#include "trident_regs.h"

static void
TRIDENTI2CPutBits(I2CBusPtr b, int clock, int data) {
    unsigned int reg = 0x0C;
    TRIDENTPtr pTrident = ((TRIDENTPtr)b->DriverPrivate.ptr);
    int vgaIOBase = VGAHWPTR(pTrident->pScrn)->IOBase;
    
#if 0
    if(!TRIDENTI2CSwitchToBus(b))
        return FALSE;
#endif

    if(clock) reg |= 2;
    if(data)  reg |= 1;
    OUTB(vgaIOBase + 4, I2C);
    OUTB(vgaIOBase + 5, reg);
#if 0
    ErrorF("TRIDENTI2CPutBits: %d %d\n", clock, data);
#endif
}

static void
TRIDENTI2CGetBits(I2CBusPtr b, int *clock, int *data) {
    unsigned int reg;
    TRIDENTPtr pTrident = ((TRIDENTPtr)b->DriverPrivate.ptr);
    int vgaIOBase = VGAHWPTR(pTrident->pScrn)->IOBase;

#if 0
    if(!TRIDENTI2CSwitchToBus(b))
        return FALSE;
#endif

    OUTB(vgaIOBase + 4, I2C);
    reg = INB(vgaIOBase + 5);
    *clock = (reg & 0x02) != 0;
    *data  = (reg & 0x01) != 0;
#if 0
    ErrorF("TRIDENTI2CGetBits: %d %d\n", *clock, *data); 
#endif
}

Bool 
TRIDENTI2CInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();

    pTrident->DDC = I2CPtr;

    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = TRIDENTI2CPutBits;
    I2CPtr->I2CGetBits = TRIDENTI2CGetBits;
    I2CPtr->DriverPrivate.ptr = pTrident;
    
    if(!xf86I2CBusInit(I2CPtr))
       return FALSE;

    return TRUE;
}
