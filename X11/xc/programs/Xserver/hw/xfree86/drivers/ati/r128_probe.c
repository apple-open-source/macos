/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/r128_probe.c,v 1.18 2003/02/09 15:33:17 tsi Exp $ */
/*
 * Copyright 1999, 2000 ATI Technologies Inc., Markham, Ontario,
 *                      Precision Insight, Inc., Cedar Park, Texas, and
 *                      VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, PRECISION INSIGHT, VA LINUX
 * SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Rickard E. Faith <faith@valinux.com>
 *   Kevin E. Martin <martin@valinux.com>
 *
 * Modified by Marc Aurele La France <tsi@xfree86.org> for ATI driver merge.
 */

#include "atimodule.h"
#include "ativersion.h"

#include "r128_probe.h"
#include "r128_version.h"

#include "xf86PciInfo.h"

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86Resources.h"

#ifdef XFree86LOADER

/*
 * The following exists to prevent the compiler from considering entry points
 * defined in a separate module from being constants.
 */
static xf86PreInitProc     * const volatile PreInitProc     = R128PreInit;
static xf86ScreenInitProc  * const volatile ScreenInitProc  = R128ScreenInit;
static xf86SwitchModeProc  * const volatile SwitchModeProc  = R128SwitchMode;
static xf86AdjustFrameProc * const volatile AdjustFrameProc = R128AdjustFrame;
static xf86EnterVTProc     * const volatile EnterVTProc     = R128EnterVT;
static xf86LeaveVTProc     * const volatile LeaveVTProc     = R128LeaveVT;
static xf86FreeScreenProc  * const volatile FreeScreenProc  = R128FreeScreen;
static xf86ValidModeProc   * const volatile ValidModeProc   = R128ValidMode;

#define R128PreInit     PreInitProc
#define R128ScreenInit  ScreenInitProc
#define R128SwitchMode  SwitchModeProc
#define R128AdjustFrame AdjustFrameProc
#define R128EnterVT     EnterVTProc
#define R128LeaveVT     LeaveVTProc
#define R128FreeScreen  FreeScreenProc
#define R128ValidMode   ValidModeProc

#endif

SymTabRec R128Chipsets[] = {
    /* FIXME: The chipsets with (PCI/AGP) are not known wether they are AGP or
     *        PCI, so I've labeled them as such in hopes users will submit
     *        data if we're unable to gather it from official documentation
     */
    { PCI_CHIP_RAGE128LE, "ATI Rage 128 Mobility M3 LE (PCI)" },
    { PCI_CHIP_RAGE128LF, "ATI Rage 128 Mobility M3 LF (AGP)" },
    { PCI_CHIP_RAGE128MF, "ATI Rage 128 Mobility M4 MF (AGP)" },
    { PCI_CHIP_RAGE128ML, "ATI Rage 128 Mobility M4 ML (AGP)" },
    { PCI_CHIP_RAGE128PA, "ATI Rage 128 Pro GL PA (PCI/AGP)" },
    { PCI_CHIP_RAGE128PB, "ATI Rage 128 Pro GL PB (PCI/AGP)" },
    { PCI_CHIP_RAGE128PC, "ATI Rage 128 Pro GL PC (PCI/AGP)" },
    { PCI_CHIP_RAGE128PD, "ATI Rage 128 Pro GL PD (PCI)" },
    { PCI_CHIP_RAGE128PE, "ATI Rage 128 Pro GL PE (PCI/AGP)" },
    { PCI_CHIP_RAGE128PF, "ATI Rage 128 Pro GL PF (AGP)" },
    { PCI_CHIP_RAGE128PG, "ATI Rage 128 Pro VR PG (PCI/AGP)" },
    { PCI_CHIP_RAGE128PH, "ATI Rage 128 Pro VR PH (PCI/AGP)" },
    { PCI_CHIP_RAGE128PI, "ATI Rage 128 Pro VR PI (PCI/AGP)" },
    { PCI_CHIP_RAGE128PJ, "ATI Rage 128 Pro VR PJ (PCI/AGP)" },
    { PCI_CHIP_RAGE128PK, "ATI Rage 128 Pro VR PK (PCI/AGP)" },
    { PCI_CHIP_RAGE128PL, "ATI Rage 128 Pro VR PL (PCI/AGP)" },
    { PCI_CHIP_RAGE128PM, "ATI Rage 128 Pro VR PM (PCI/AGP)" },
    { PCI_CHIP_RAGE128PN, "ATI Rage 128 Pro VR PN (PCI/AGP)" },
    { PCI_CHIP_RAGE128PO, "ATI Rage 128 Pro VR PO (PCI/AGP)" },
    { PCI_CHIP_RAGE128PP, "ATI Rage 128 Pro VR PP (PCI)" },
    { PCI_CHIP_RAGE128PQ, "ATI Rage 128 Pro VR PQ (PCI/AGP)" },
    { PCI_CHIP_RAGE128PR, "ATI Rage 128 Pro VR PR (PCI)" },
    { PCI_CHIP_RAGE128PS, "ATI Rage 128 Pro VR PS (PCI/AGP)" },
    { PCI_CHIP_RAGE128PT, "ATI Rage 128 Pro VR PT (PCI/AGP)" },
    { PCI_CHIP_RAGE128PU, "ATI Rage 128 Pro VR PU (PCI/AGP)" },
    { PCI_CHIP_RAGE128PV, "ATI Rage 128 Pro VR PV (PCI/AGP)" },
    { PCI_CHIP_RAGE128PW, "ATI Rage 128 Pro VR PW (PCI/AGP)" },
    { PCI_CHIP_RAGE128PX, "ATI Rage 128 Pro VR PX (PCI/AGP)" },
    { PCI_CHIP_RAGE128RE, "ATI Rage 128 GL RE (PCI)" },
    { PCI_CHIP_RAGE128RF, "ATI Rage 128 GL RF (AGP)" },
    { PCI_CHIP_RAGE128RG, "ATI Rage 128 RG (AGP)" },
    { PCI_CHIP_RAGE128RK, "ATI Rage 128 VR RK (PCI)" },
    { PCI_CHIP_RAGE128RL, "ATI Rage 128 VR RL (AGP)" },
    { PCI_CHIP_RAGE128SE, "ATI Rage 128 4X SE (PCI/AGP)" },
    { PCI_CHIP_RAGE128SF, "ATI Rage 128 4X SF (PCI/AGP)" },
    { PCI_CHIP_RAGE128SG, "ATI Rage 128 4X SG (PCI/AGP)" },
    { PCI_CHIP_RAGE128SH, "ATI Rage 128 4X SH (PCI/AGP)" },
    { PCI_CHIP_RAGE128SK, "ATI Rage 128 4X SK (PCI/AGP)" },
    { PCI_CHIP_RAGE128SL, "ATI Rage 128 4X SL (PCI/AGP)" },
    { PCI_CHIP_RAGE128SM, "ATI Rage 128 4X SM (AGP)" },
    { PCI_CHIP_RAGE128SN, "ATI Rage 128 4X SN (PCI/AGP)" },
    { PCI_CHIP_RAGE128TF, "ATI Rage 128 Pro ULTRA TF (AGP)" },
    { PCI_CHIP_RAGE128TL, "ATI Rage 128 Pro ULTRA TL (AGP)" },
    { PCI_CHIP_RAGE128TR, "ATI Rage 128 Pro ULTRA TR (AGP)" },
    { PCI_CHIP_RAGE128TS, "ATI Rage 128 Pro ULTRA TS (AGP?)" },
    { PCI_CHIP_RAGE128TT, "ATI Rage 128 Pro ULTRA TT (AGP?)" },
    { PCI_CHIP_RAGE128TU, "ATI Rage 128 Pro ULTRA TU (AGP?)" },
    { -1,                 NULL }
};

PciChipsets R128PciChipsets[] = {
    { PCI_CHIP_RAGE128LE, PCI_CHIP_RAGE128LE, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128LF, PCI_CHIP_RAGE128LF, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128MF, PCI_CHIP_RAGE128MF, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128ML, PCI_CHIP_RAGE128ML, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PA, PCI_CHIP_RAGE128PA, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PB, PCI_CHIP_RAGE128PB, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PC, PCI_CHIP_RAGE128PC, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PD, PCI_CHIP_RAGE128PD, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PE, PCI_CHIP_RAGE128PE, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PF, PCI_CHIP_RAGE128PF, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PG, PCI_CHIP_RAGE128PG, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PH, PCI_CHIP_RAGE128PH, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PI, PCI_CHIP_RAGE128PI, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PJ, PCI_CHIP_RAGE128PJ, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PK, PCI_CHIP_RAGE128PK, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PL, PCI_CHIP_RAGE128PL, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PM, PCI_CHIP_RAGE128PM, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PN, PCI_CHIP_RAGE128PN, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PO, PCI_CHIP_RAGE128PO, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PP, PCI_CHIP_RAGE128PP, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PQ, PCI_CHIP_RAGE128PQ, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PR, PCI_CHIP_RAGE128PR, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PS, PCI_CHIP_RAGE128PS, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PT, PCI_CHIP_RAGE128PT, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PU, PCI_CHIP_RAGE128PU, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PV, PCI_CHIP_RAGE128PV, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PW, PCI_CHIP_RAGE128PW, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128PX, PCI_CHIP_RAGE128PX, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128RE, PCI_CHIP_RAGE128RE, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128RF, PCI_CHIP_RAGE128RF, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128RG, PCI_CHIP_RAGE128RG, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128RK, PCI_CHIP_RAGE128RK, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128RL, PCI_CHIP_RAGE128RL, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SE, PCI_CHIP_RAGE128SE, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SF, PCI_CHIP_RAGE128SF, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SG, PCI_CHIP_RAGE128SG, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SH, PCI_CHIP_RAGE128SH, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SK, PCI_CHIP_RAGE128SK, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SL, PCI_CHIP_RAGE128SL, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SM, PCI_CHIP_RAGE128SM, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128SN, PCI_CHIP_RAGE128SN, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128TF, PCI_CHIP_RAGE128TF, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128TL, PCI_CHIP_RAGE128TL, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128TR, PCI_CHIP_RAGE128TR, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128TS, PCI_CHIP_RAGE128TS, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128TT, PCI_CHIP_RAGE128TT, RES_SHARED_VGA },
    { PCI_CHIP_RAGE128TU, PCI_CHIP_RAGE128TU, RES_SHARED_VGA },
    { -1,                 -1,                 RES_UNDEFINED }
};

/* Return the options for supported chipset 'n'; NULL otherwise */
const OptionInfoRec *
R128AvailableOptions(int chipid, int busid)
{
    int i;

    /*
     * Return options defined in the r128 submodule which will have been
     * loaded by this point.
     */
    if ((chipid >> 16) == PCI_VENDOR_ATI)
	chipid -= PCI_VENDOR_ATI << 16;
    for (i = 0; R128PciChipsets[i].PCIid > 0; i++) {
	if (chipid == R128PciChipsets[i].PCIid)
	    return R128Options;
    }
    return NULL;
}

/* Return the string name for supported chipset 'n'; NULL otherwise. */
void
R128Identify(int flags)
{
    xf86PrintChipsets(R128_NAME,
		      "Driver for ATI Rage 128 chipsets",
		      R128Chipsets);
}

/* Return TRUE if chipset is present; FALSE otherwise. */
Bool
R128Probe(DriverPtr drv, int flags)
{
    int           numUsed;
    int           numDevSections, nATIGDev, nR128GDev;
    int           *usedChips;
    GDevPtr       *devSections, *ATIGDevs, *R128GDevs;
    EntityInfoPtr pEnt;
    Bool          foundScreen = FALSE;
    int           i;

    if (!xf86GetPciVideoInfo()) return FALSE;

    /* Collect unclaimed device sections for both driver names */
    nATIGDev = xf86MatchDevice(ATI_NAME, &ATIGDevs);
    nR128GDev = xf86MatchDevice(R128_NAME, &R128GDevs);

    if (!(numDevSections = nATIGDev + nR128GDev)) return FALSE;

    if (!ATIGDevs) {
	if (!(devSections = R128GDevs))
	    numDevSections = 1;
	else
	    numDevSections = nR128GDev;
    } if (!R128GDevs) {
	devSections = ATIGDevs;
	numDevSections = nATIGDev;
    } else {
	/* Combine into one list */
	devSections = xnfalloc((numDevSections + 1) * sizeof(GDevPtr));
	(void)memcpy(devSections,
		     ATIGDevs, nATIGDev * sizeof(GDevPtr));
	(void)memcpy(devSections + nATIGDev,
		     R128GDevs, nR128GDev * sizeof(GDevPtr));
	devSections[numDevSections] = NULL;
	xfree(ATIGDevs);
	xfree(R128GDevs);
    }

    numUsed = xf86MatchPciInstances(R128_NAME,
				    PCI_VENDOR_ATI,
				    R128Chipsets,
				    R128PciChipsets,
				    devSections,
				    numDevSections,
				    drv,
				    &usedChips);

    if (numUsed<=0) return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
	pEnt = xf86GetEntityInfo(usedChips[i]);

	if (pEnt->active) {
	    ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);

#ifdef XFree86LOADER

	    if (!xf86LoadSubModule(pScrn, "r128")) {
		xf86Msg(X_ERROR,
		    R128_NAME ":  Failed to load \"r128\" module.\n");
		xf86DeleteScreen(pScrn->scrnIndex, 0);
		continue;
	    }

	    xf86LoaderReqSymLists(R128Symbols, NULL);

#endif

	    pScrn->driverVersion = R128_VERSION_CURRENT;
	    pScrn->driverName    = R128_DRIVER_NAME;
	    pScrn->name          = R128_NAME;
	    pScrn->Probe         = R128Probe;
	    pScrn->PreInit       = R128PreInit;
	    pScrn->ScreenInit    = R128ScreenInit;
	    pScrn->SwitchMode    = R128SwitchMode;
	    pScrn->AdjustFrame   = R128AdjustFrame;
	    pScrn->EnterVT       = R128EnterVT;
	    pScrn->LeaveVT       = R128LeaveVT;
	    pScrn->FreeScreen    = R128FreeScreen;
	    pScrn->ValidMode     = R128ValidMode;

	    foundScreen          = TRUE;

	    xf86ConfigActivePciEntity(pScrn, usedChips[i], R128PciChipsets,
				      0, 0, 0, 0, 0);
	}
	xfree(pEnt);
    }

    xfree(usedChips);
    xfree(devSections);

    return foundScreen;
}
