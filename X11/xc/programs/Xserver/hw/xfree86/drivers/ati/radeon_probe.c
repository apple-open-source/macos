/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_probe.c,v 1.24 2003/02/07 20:41:15 martin Exp $ */
/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
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
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *
 * Modified by Marc Aurele La France <tsi@xfree86.org> for ATI driver merge.
 */

#include "atimodule.h"
#include "ativersion.h"

#include "radeon_probe.h"
#include "radeon_version.h"

#include "xf86PciInfo.h"

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86Resources.h"

#ifdef XFree86LOADER

/*
 * The following exists to prevent the compiler from considering entry points
 * defined in a separate module from being constants.
 */
static xf86PreInitProc     *const volatile PreInitProc     = RADEONPreInit;
static xf86ScreenInitProc  *const volatile ScreenInitProc  = RADEONScreenInit;
static xf86SwitchModeProc  *const volatile SwitchModeProc  = RADEONSwitchMode;
static xf86AdjustFrameProc *const volatile AdjustFrameProc = RADEONAdjustFrame;
static xf86EnterVTProc     *const volatile EnterVTProc     = RADEONEnterVT;
static xf86LeaveVTProc     *const volatile LeaveVTProc     = RADEONLeaveVT;
static xf86FreeScreenProc  *const volatile FreeScreenProc  = RADEONFreeScreen;
static xf86ValidModeProc   *const volatile ValidModeProc   = RADEONValidMode;

#define RADEONPreInit     PreInitProc
#define RADEONScreenInit  ScreenInitProc
#define RADEONSwitchMode  SwitchModeProc
#define RADEONAdjustFrame AdjustFrameProc
#define RADEONEnterVT     EnterVTProc
#define RADEONLeaveVT     LeaveVTProc
#define RADEONFreeScreen  FreeScreenProc
#define RADEONValidMode   ValidModeProc

#endif

SymTabRec RADEONChipsets[] = {
    { PCI_CHIP_RADEON_QD, "ATI Radeon QD (AGP)" },
    { PCI_CHIP_RADEON_QE, "ATI Radeon QE (AGP)" },
    { PCI_CHIP_RADEON_QF, "ATI Radeon QF (AGP)" },
    { PCI_CHIP_RADEON_QG, "ATI Radeon QG (AGP)" },
    { PCI_CHIP_RV100_QY, "ATI Radeon VE/7000 QY (AGP)" },
    { PCI_CHIP_RV100_QZ, "ATI Radeon VE/7000 QZ (AGP)" },
    { PCI_CHIP_RADEON_LW, "ATI Radeon Mobility M7 LW (AGP)" },
    { PCI_CHIP_RADEON_LX, "ATI Mobility FireGL 7800 M7 LX (AGP)" },
    { PCI_CHIP_RADEON_LY, "ATI Radeon Mobility M6 LY (AGP)" },
    { PCI_CHIP_RADEON_LZ, "ATI Radeon Mobility M6 LZ (AGP)" },
    { PCI_CHIP_R200_QH, "ATI FireGL 8700/8800 QH (AGP)" },
    { PCI_CHIP_R200_QI, "ATI Radeon 8500 QI (AGP)" },
    { PCI_CHIP_R200_QJ, "ATI Radeon 8500 QJ (AGP)" },
    { PCI_CHIP_R200_QK, "ATI Radeon 8500 QK (AGP)" },
    { PCI_CHIP_R200_QL, "ATI Radeon 8500 QL (AGP)" },
    { PCI_CHIP_R200_QM, "ATI Radeon 9100 QM (AGP)" },
    { PCI_CHIP_R200_QN, "ATI Radeon 8500 QN (AGP)" },
    { PCI_CHIP_R200_QO, "ATI Radeon 8500 QO (AGP)" },
    { PCI_CHIP_R200_Qh, "ATI Radeon 8500 Qh (AGP)" },
    { PCI_CHIP_R200_Qi, "ATI Radeon 8500 Qi (AGP)" },
    { PCI_CHIP_R200_Qj, "ATI Radeon 8500 Qj (AGP)" },
    { PCI_CHIP_R200_Qk, "ATI Radeon 8500 Qk (AGP)" },
    { PCI_CHIP_R200_Ql, "ATI Radeon 8500 Ql (AGP)" },
    { PCI_CHIP_R200_BB, "ATI Radeon 8500 BB (AGP)" },
    { PCI_CHIP_RV200_QW, "ATI Radeon 7500 QW (AGP)" },
    { PCI_CHIP_RV200_QX, "ATI Radeon 7500 QX (AGP)" },
    { PCI_CHIP_RV250_Id, "ATI Radeon 9000 Id (AGP)" },
    { PCI_CHIP_RV250_Ie, "ATI Radeon 9000 Ie (AGP)" },
    { PCI_CHIP_RV250_If, "ATI Radeon 9000 If (AGP)" },
    { PCI_CHIP_RV250_Ig, "ATI Radeon 9000 Ig (AGP)" },
    { PCI_CHIP_RV250_Ld, "ATI Radeon Mobility M9 Ld (AGP)" },
    { PCI_CHIP_RV250_Le, "ATI Radeon Mobility M9 Le (AGP)" },
    { PCI_CHIP_RV250_Lf, "ATI Radeon Mobility M9 Lf (AGP)" },
    { PCI_CHIP_RV250_Lg, "ATI Radeon Mobility M9 Lg (AGP)" },
    { PCI_CHIP_R300_AD, "ATI Radeon 9500 AD (AGP)" },
    { PCI_CHIP_R300_AE, "ATI Radeon 9500 AE (AGP)" },
    { PCI_CHIP_R300_AF, "ATI Radeon 9500 AF (AGP)" },
    { PCI_CHIP_R300_AG, "ATI FireGL Z1/X1 AG (AGP)" },
    { PCI_CHIP_R300_ND, "ATI Radeon 9700 Pro ND (AGP)" },
    { PCI_CHIP_R300_NE, "ATI Radeon 9700/9500Pro NE (AGP)" },
    { PCI_CHIP_R300_NF, "ATI Radeon 9700 NF (AGP)" },
    { PCI_CHIP_R300_NG, "ATI FireGL X1 NG (AGP)" },
    { -1,                 NULL }
};

PciChipsets RADEONPciChipsets[] = {
    { PCI_CHIP_RADEON_QD, PCI_CHIP_RADEON_QD, RES_SHARED_VGA },
    { PCI_CHIP_RADEON_QE, PCI_CHIP_RADEON_QE, RES_SHARED_VGA },
    { PCI_CHIP_RADEON_QF, PCI_CHIP_RADEON_QF, RES_SHARED_VGA },
    { PCI_CHIP_RADEON_QG, PCI_CHIP_RADEON_QG, RES_SHARED_VGA },
    { PCI_CHIP_RV100_QY, PCI_CHIP_RV100_QY, RES_SHARED_VGA },
    { PCI_CHIP_RV100_QZ, PCI_CHIP_RV100_QZ, RES_SHARED_VGA },
    { PCI_CHIP_RADEON_LW, PCI_CHIP_RADEON_LW, RES_SHARED_VGA },
    { PCI_CHIP_RADEON_LX, PCI_CHIP_RADEON_LX, RES_SHARED_VGA },
    { PCI_CHIP_RADEON_LY, PCI_CHIP_RADEON_LY, RES_SHARED_VGA },
    { PCI_CHIP_RADEON_LZ, PCI_CHIP_RADEON_LZ, RES_SHARED_VGA },
    { PCI_CHIP_R200_QH, PCI_CHIP_R200_QH, RES_SHARED_VGA },
    { PCI_CHIP_R200_QI, PCI_CHIP_R200_QI, RES_SHARED_VGA },
    { PCI_CHIP_R200_QJ, PCI_CHIP_R200_QJ, RES_SHARED_VGA },
    { PCI_CHIP_R200_QK, PCI_CHIP_R200_QK, RES_SHARED_VGA },
    { PCI_CHIP_R200_QL, PCI_CHIP_R200_QL, RES_SHARED_VGA },
    { PCI_CHIP_R200_QM, PCI_CHIP_R200_QM, RES_SHARED_VGA },
    { PCI_CHIP_R200_QN, PCI_CHIP_R200_QN, RES_SHARED_VGA },
    { PCI_CHIP_R200_QO, PCI_CHIP_R200_QO, RES_SHARED_VGA },
    { PCI_CHIP_R200_Qh, PCI_CHIP_R200_Qh, RES_SHARED_VGA },
    { PCI_CHIP_R200_Qi, PCI_CHIP_R200_Qi, RES_SHARED_VGA },
    { PCI_CHIP_R200_Qj, PCI_CHIP_R200_Qj, RES_SHARED_VGA },
    { PCI_CHIP_R200_Qk, PCI_CHIP_R200_Qk, RES_SHARED_VGA },
    { PCI_CHIP_R200_Ql, PCI_CHIP_R200_Ql, RES_SHARED_VGA },
    { PCI_CHIP_R200_BB, PCI_CHIP_R200_BB, RES_SHARED_VGA },
    { PCI_CHIP_RV200_QW, PCI_CHIP_RV200_QW, RES_SHARED_VGA },
    { PCI_CHIP_RV200_QX, PCI_CHIP_RV200_QX, RES_SHARED_VGA },
    { PCI_CHIP_RV250_Id, PCI_CHIP_RV250_Id, RES_SHARED_VGA },
    { PCI_CHIP_RV250_Ie, PCI_CHIP_RV250_Ie, RES_SHARED_VGA },
    { PCI_CHIP_RV250_If, PCI_CHIP_RV250_If, RES_SHARED_VGA },
    { PCI_CHIP_RV250_Ig, PCI_CHIP_RV250_Ig, RES_SHARED_VGA },
    { PCI_CHIP_RV250_Ld, PCI_CHIP_RV250_Ld, RES_SHARED_VGA },
    { PCI_CHIP_RV250_Le, PCI_CHIP_RV250_Le, RES_SHARED_VGA },
    { PCI_CHIP_RV250_Lf, PCI_CHIP_RV250_Lf, RES_SHARED_VGA },
    { PCI_CHIP_RV250_Lg, PCI_CHIP_RV250_Lg, RES_SHARED_VGA },
    { PCI_CHIP_R300_AD, PCI_CHIP_R300_AD, RES_SHARED_VGA },
    { PCI_CHIP_R300_AE, PCI_CHIP_R300_AE, RES_SHARED_VGA },
    { PCI_CHIP_R300_AF, PCI_CHIP_R300_AF, RES_SHARED_VGA },
    { PCI_CHIP_R300_AG, PCI_CHIP_R300_AG, RES_SHARED_VGA },
    { PCI_CHIP_R300_ND, PCI_CHIP_R300_ND, RES_SHARED_VGA },
    { PCI_CHIP_R300_NE, PCI_CHIP_R300_NE, RES_SHARED_VGA },
    { PCI_CHIP_R300_NF, PCI_CHIP_R300_NF, RES_SHARED_VGA },
    { PCI_CHIP_R300_NG, PCI_CHIP_R300_NG, RES_SHARED_VGA },
    { -1,                 -1,                 RES_UNDEFINED }
};

int gRADEONEntityIndex = -1;

/* Return the options for supported chipset 'n'; NULL otherwise */
const OptionInfoRec *
RADEONAvailableOptions(int chipid, int busid)
{
    int  i;

    /*
     * Return options defined in the radeon submodule which will have been
     * loaded by this point.
     */
    if ((chipid >> 16) == PCI_VENDOR_ATI)
	chipid -= PCI_VENDOR_ATI << 16;
    for (i = 0; RADEONPciChipsets[i].PCIid > 0; i++) {
	if (chipid == RADEONPciChipsets[i].PCIid)
	    return RADEONOptions;
    }
    return NULL;
}

/* Return the string name for supported chipset 'n'; NULL otherwise. */
void
RADEONIdentify(int flags)
{
    xf86PrintChipsets(RADEON_NAME,
		      "Driver for ATI Radeon chipsets",
		      RADEONChipsets);
}

/* Return TRUE if chipset is present; FALSE otherwise. */
Bool
RADEONProbe(DriverPtr drv, int flags)
{
    int      numUsed;
    int      numDevSections, nATIGDev, nRadeonGDev;
    int     *usedChips;
    GDevPtr *devSections, *ATIGDevs, *RadeonGDevs;
    Bool     foundScreen = FALSE;
    int      i;

    if (!xf86GetPciVideoInfo()) return FALSE;

    /* Collect unclaimed device sections for both driver names */
    nATIGDev    = xf86MatchDevice(ATI_NAME, &ATIGDevs);
    nRadeonGDev = xf86MatchDevice(RADEON_NAME, &RadeonGDevs);

    if (!(numDevSections = nATIGDev + nRadeonGDev)) return FALSE;

    if (!ATIGDevs) {
	if (!(devSections = RadeonGDevs)) numDevSections = 1;
	else                              numDevSections = nRadeonGDev;
    } if (!RadeonGDevs) {
	devSections    = ATIGDevs;
	numDevSections = nATIGDev;
    } else {
	/* Combine into one list */
	devSections = xnfalloc((numDevSections + 1) * sizeof(GDevPtr));
	(void)memcpy(devSections,
		     ATIGDevs, nATIGDev * sizeof(GDevPtr));
	(void)memcpy(devSections + nATIGDev,
		     RadeonGDevs, nRadeonGDev * sizeof(GDevPtr));
	devSections[numDevSections] = NULL;
	xfree(ATIGDevs);
	xfree(RadeonGDevs);
    }

    numUsed = xf86MatchPciInstances(RADEON_NAME,
				    PCI_VENDOR_ATI,
				    RADEONChipsets,
				    RADEONPciChipsets,
				    devSections,
				    numDevSections,
				    drv,
				    &usedChips);

    if (numUsed <= 0) return FALSE;

    if (flags & PROBE_DETECT) {
	foundScreen = TRUE;
    } else {
	for (i = 0; i < numUsed; i++) {
	    ScrnInfoPtr    pScrn = NULL;
	    EntityInfoPtr  pEnt;

	    if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
					     RADEONPciChipsets, 0, 0, 0,
					     0, 0))) {
#ifdef XFree86LOADER
		if (!xf86LoadSubModule(pScrn, "radeon")) {
		    xf86Msg(X_ERROR, RADEON_NAME
			    ":  Failed to load \"radeon\" module.\n");
		    xf86DeleteScreen(pScrn->scrnIndex, 0);
		    continue;
		}

		xf86LoaderReqSymLists(RADEONSymbols, NULL);
#endif

		pScrn->driverVersion = RADEON_VERSION_CURRENT;
		pScrn->driverName    = RADEON_DRIVER_NAME;
		pScrn->name          = RADEON_NAME;
		pScrn->Probe         = RADEONProbe;
		pScrn->PreInit       = RADEONPreInit;
		pScrn->ScreenInit    = RADEONScreenInit;
		pScrn->SwitchMode    = RADEONSwitchMode;
		pScrn->AdjustFrame   = RADEONAdjustFrame;
		pScrn->EnterVT       = RADEONEnterVT;
		pScrn->LeaveVT       = RADEONLeaveVT;
		pScrn->FreeScreen    = RADEONFreeScreen;
		pScrn->ValidMode     = RADEONValidMode;
		foundScreen          = TRUE;
	    }

	    pEnt = xf86GetEntityInfo(usedChips[i]);

	    /* All Radeon chips except the original ones support
	     * Dual-Head, mark the entity as sharable.
	     */
	    if (pEnt->chipset != PCI_CHIP_RADEON_QD &&
		pEnt->chipset != PCI_CHIP_RADEON_QE &&
		pEnt->chipset != PCI_CHIP_RADEON_QF &&
		pEnt->chipset != PCI_CHIP_RADEON_QG) {
		static int  instance = 0;
		DevUnion   *pPriv;

		xf86SetEntitySharable(usedChips[i]);
		xf86SetEntityInstanceForScreen(pScrn, pScrn->entityList[0],
					       instance);

		if (gRADEONEntityIndex < 0) {
		    gRADEONEntityIndex = xf86AllocateEntityPrivateIndex();
		    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
						 gRADEONEntityIndex);

		    if (!pPriv->ptr) {
			RADEONEntPtr pRADEONEnt;

			pPriv->ptr = xnfcalloc(sizeof(RADEONEntRec), 1);
			pRADEONEnt = pPriv->ptr;
			pRADEONEnt->IsDRIEnabled = FALSE;
			pRADEONEnt->BypassSecondary = FALSE;
			pRADEONEnt->HasSecondary = FALSE;
			pRADEONEnt->IsSecondaryRestored = FALSE;
		    }
		}
		instance++;
		if (instance == 2) {
		    RADEONEntPtr  pRADEONEnt;

		    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
						 gRADEONEntityIndex);
		    pRADEONEnt = pPriv->ptr;		
		    pRADEONEnt->HasSecondary = TRUE;
		}

	    }
	    xfree(pEnt);
	}
    }

    xfree(usedChips);
    xfree(devSections);

    return foundScreen;
}
