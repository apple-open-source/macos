/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Copyright (c) 2003 Stuart Walsh<stu@ipng.org.uk>
 * and Duncan Barclay<dmlb@dmlb.org>
 */

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS 'AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IOLib.h>
#include "if_bfereg.h"

#define DEBUG_LOG(fmt, args...)  IOLog(fmt, ## args)

#ifndef DELAY(x)
#define DELAY(x)  IODelay(x)
#endif

static void bfe_pci_setup( struct bfe_softc * sc, u_int32_t cores );
static void bfe_clear_stats( struct bfe_softc * sc );
static void bfe_core_disable( struct bfe_softc * sc );
static void bfe_core_reset( struct bfe_softc * sc );
static int  bfe_wait_bit( struct bfe_softc * sc, u_int32_t reg, u_int32_t bit, 
                          u_long timeout, const int clear );

static void
bfe_pci_setup( struct bfe_softc * sc, u_int32_t cores )
{
	u_int32_t bar_orig, pci_rev, val;

	bar_orig = pci_read_config(sc->bfe_dev, BFE_BAR0_WIN, 4);
	pci_write_config(sc->bfe_dev, BFE_BAR0_WIN, BFE_REG_PCI, 4);
	pci_rev = CSR_READ_4(sc, BFE_SBIDHIGH) & BFE_RC_MASK;

	val = CSR_READ_4(sc, BFE_SBINTVEC);
	val |= cores;
	CSR_WRITE_4(sc, BFE_SBINTVEC, val);

	val = CSR_READ_4(sc, BFE_SSB_PCI_TRANS_2);
	val |= BFE_SSB_PCI_PREF | BFE_SSB_PCI_BURST;
	CSR_WRITE_4(sc, BFE_SSB_PCI_TRANS_2, val);

	pci_write_config(sc->bfe_dev, BFE_BAR0_WIN, bar_orig, 4);
}

static void 
bfe_clear_stats( struct bfe_softc * sc )
{
	u_long reg;

	BFE_LOCK(sc);

	CSR_WRITE_4(sc, BFE_MIB_CTRL, BFE_MIB_CLR_ON_READ);
	for (reg = BFE_TX_GOOD_O; reg <= BFE_TX_PAUSE; reg += 4)
		CSR_READ_4(sc, reg);
	for (reg = BFE_RX_GOOD_O; reg <= BFE_RX_NPAUSE; reg += 4)
		CSR_READ_4(sc, reg);

	BFE_UNLOCK(sc);
}

__private_extern__ void
bfe_chip_halt( struct bfe_softc * sc )
{
	BFE_LOCK(sc);
	/* disable interrupts - not that it actually does..*/
	CSR_WRITE_4(sc, BFE_IMASK, 0);
	CSR_READ_4(sc, BFE_IMASK);

    CSR_WRITE_4(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE);
	bfe_wait_bit(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE, 200, 1);

	CSR_WRITE_4(sc, BFE_DMARX_CTRL, 0);
	CSR_WRITE_4(sc, BFE_DMATX_CTRL, 0);
	DELAY(10);

	BFE_UNLOCK(sc);
}

__private_extern__ void
bfe_chip_reset( struct bfe_softc * sc )
{
	u_int32_t val;    

	BFE_LOCK(sc);

	/* Set the interrupt vector for the enet core */
	bfe_pci_setup(sc, BFE_INTVEC_ENET0);

	/* is core up? */
	val = CSR_READ_4(sc, BFE_SBTMSLOW) & (BFE_RESET | BFE_REJECT | BFE_CLOCK);
	if (val == BFE_CLOCK) {
		/* It is, so shut it down */
		CSR_WRITE_4(sc, BFE_RCV_LAZY, 0);
		CSR_WRITE_4(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE);
		bfe_wait_bit(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE, 100, 1);
		CSR_WRITE_4(sc, BFE_DMATX_CTRL, 0);
#ifndef __APPLE__
		sc->bfe_tx_cnt = sc->bfe_tx_prod = sc->bfe_tx_cons = 0;
#endif
		if (CSR_READ_4(sc, BFE_DMARX_STAT) & BFE_STAT_EMASK) 
			bfe_wait_bit(sc, BFE_DMARX_STAT, BFE_STAT_SIDLE, 100, 0);
		CSR_WRITE_4(sc, BFE_DMARX_CTRL, 0);
#ifndef __APPLE__
		sc->bfe_rx_prod = sc->bfe_rx_cons = 0;
#endif
	}

	bfe_core_reset(sc);
	bfe_clear_stats(sc);

	/*
	 * We want the phy registers to be accessible even when
	 * the driver is "downed" so initialize MDC preamble, frequency,
	 * and whether internal or external phy here.
	 */

	/* 4402 has 62.5Mhz SB clock and internal phy */
	CSR_WRITE_4(sc, BFE_MDIO_CTRL, 0x8d);

	/* Internal or external PHY? */
	val = CSR_READ_4(sc, BFE_DEVCTRL);
	if(!(val & BFE_IPP)) {
        /* external PHY */
		CSR_WRITE_4(sc, BFE_ENET_CTRL, BFE_ENET_EPSEL);
		CSR_READ_4(sc, BFE_ENET_CTRL);
    }
    else if(CSR_READ_4(sc, BFE_DEVCTRL) & BFE_EPR) {
        /* internal PHY */
		BFE_AND(sc, BFE_DEVCTRL, ~BFE_EPR);
		CSR_READ_4(sc, BFE_DEVCTRL);
		DELAY(100);
	}

	BFE_OR(sc, BFE_MAC_CTRL, BFE_CTRL_CRC32_ENAB | BFE_CTRL_LED);

#ifdef __APPLE__
	CSR_WRITE_4(sc, BFE_RCV_LAZY, (3 << BFE_LAZY_FC_SHIFT) | 0x3000);
#else
	CSR_WRITE_4(sc, BFE_RCV_LAZY, ((1 << BFE_LAZY_FC_SHIFT) & 
				BFE_LAZY_FC_MASK));

	/* 
	 * We don't want lazy interrupts, so just send them at the end of a frame,
	 * please 
	 */
	BFE_OR(sc, BFE_RCV_LAZY, 0);
#endif

	/* Set max lengths, accounting for VLAN tags */
	CSR_WRITE_4(sc, BFE_RXMAXLEN, BFE_BUFFER_SIZE);
	CSR_WRITE_4(sc, BFE_TXMAXLEN, BFE_BUFFER_SIZE);

	/* Set watermark XXX - magic */
	CSR_WRITE_4(sc, BFE_TX_WMARK, 56);

	/* 
	 * Initialise DMA channels - not forgetting dma addresses need to be added
	 * to BFE_PCI_DMA 
	 */
	CSR_WRITE_4(sc, BFE_DMATX_CTRL, BFE_TX_CTRL_ENABLE);
	CSR_WRITE_4(sc, BFE_DMATX_ADDR, sc->bfe_tx_dma + BFE_PCI_DMA);

	CSR_WRITE_4(sc, BFE_DMARX_CTRL, (BFE_RX_OFFSET << BFE_RX_CTRL_ROSHIFT) | 
			BFE_RX_CTRL_ENABLE);
	CSR_WRITE_4(sc, BFE_DMARX_ADDR, sc->bfe_rx_dma + BFE_PCI_DMA);

    CSR_WRITE_4(sc, BFE_MIB_CTRL, BFE_MIB_CLR_ON_READ);

#ifndef __APPLE__
	bfe_resetphy(sc);
	bfe_setupphy(sc);
#endif

	BFE_UNLOCK(sc);
}

static void
bfe_core_disable( struct bfe_softc * sc )
{
	if((CSR_READ_4(sc, BFE_SBTMSLOW)) & BFE_RESET)
		return;

	/* 
	 * Set reject, wait for it set, then wait for the core to stop being busy
	 * Then set reset and reject and enable the clocks
	 */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_REJECT | BFE_CLOCK));
	bfe_wait_bit(sc, BFE_SBTMSLOW, BFE_REJECT, 100000, 0);
	bfe_wait_bit(sc, BFE_SBTMSHIGH, BFE_BUSY, 100000, 1);
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_FGC | BFE_CLOCK | BFE_REJECT |
				BFE_RESET));
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);
	/* Leave reset and reject set */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_REJECT | BFE_RESET));
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);
}

static void
bfe_core_reset( struct bfe_softc * sc )
{
	u_int32_t val;

	/* Disable the core */
	bfe_core_disable(sc);

	/* and bring it back up */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_RESET | BFE_CLOCK | BFE_FGC));
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);

	/* Chip bug, clear SERR, IB and TO if they are set. */
	if (CSR_READ_4(sc, BFE_SBTMSHIGH) & BFE_SERR)
		CSR_WRITE_4(sc, BFE_SBTMSHIGH, 0);
	val = CSR_READ_4(sc, BFE_SBIMSTATE);
	if (val & (BFE_IBE | BFE_TO))
		CSR_WRITE_4(sc, BFE_SBIMSTATE, val & ~(BFE_IBE | BFE_TO));

	/* Clear reset and allow it to move through the core */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_CLOCK | BFE_FGC));
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);

	/* Leave the clock set */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, BFE_CLOCK);
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);
}

__private_extern__ void 
bfe_cam_write( struct bfe_softc * sc, u_char * data, int index )
{
	u_int32_t val;

	val  = ((u_int32_t) data[2]) << 24;
	val |= ((u_int32_t) data[3]) << 16;
	val |= ((u_int32_t) data[4]) <<  8;
	val |= ((u_int32_t) data[5]);
	CSR_WRITE_4(sc, BFE_CAM_DATA_LO, val);
	val = (BFE_CAM_HI_VALID |
			(((u_int32_t) data[0]) << 8) |
			(((u_int32_t) data[1])));
	CSR_WRITE_4(sc, BFE_CAM_DATA_HI, val);
	CSR_WRITE_4(sc, BFE_CAM_CTRL, (BFE_CAM_WRITE |
				(index << BFE_CAM_INDEX_SHIFT)));
	bfe_wait_bit(sc, BFE_CAM_CTRL, BFE_CAM_BUSY, 10000, 1);
}

static int
bfe_wait_bit( struct bfe_softc * sc, u_int32_t reg, u_int32_t bit, 
              u_long timeout, const int clear )
{
	u_long i;
    int use_sleep = 0;

    /* jliu@Apple: long timeouts polls are done using a blocking wait */
    if (timeout >= 10000) {
        use_sleep = 1;
        timeout /= 1000;
    }

	for (i = 0; i < timeout; i++) {
		u_int32_t val = CSR_READ_4(sc, reg);

		if (clear && !(val & bit))
			break;
		if (!clear && (val & bit))
			break;

        if (use_sleep)
            IOSleep(10);
        else
            DELAY(10);
	}
	if (i == timeout) {
		DEBUG_LOG("bfe%d: BUG!  Timeout waiting for bit %08x of register "
				  "%x to %s.\n", sc->bfe_unit, bit, reg, 
				  (clear ? "clear" : "set"));
		return -1;
	}
	return 0;
}

__private_extern__ int
bfe_readphy( struct bfe_softc * sc, u_int32_t reg, u_int32_t * val )
{
	int err; 

	BFE_LOCK(sc);
	/* Clear MII ISR */
	CSR_WRITE_4(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII);
	CSR_WRITE_4(sc, BFE_MDIO_DATA, (BFE_MDIO_SB_START |
				(BFE_MDIO_OP_READ << BFE_MDIO_OP_SHIFT) |
				(sc->bfe_phyaddr << BFE_MDIO_PMD_SHIFT) |
				(reg << BFE_MDIO_RA_SHIFT) |
				(BFE_MDIO_TA_VALID << BFE_MDIO_TA_SHIFT)));
	err = bfe_wait_bit(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII, 100, 0);
	*val = CSR_READ_4(sc, BFE_MDIO_DATA) & BFE_MDIO_DATA_DATA;

	BFE_UNLOCK(sc);
	return err;
}

__private_extern__ int
bfe_writephy( struct bfe_softc * sc, u_int32_t reg, u_int32_t val )
{
	int status;

	BFE_LOCK(sc);
	CSR_WRITE_4(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII);
	CSR_WRITE_4(sc, BFE_MDIO_DATA, (BFE_MDIO_SB_START |
				(BFE_MDIO_OP_WRITE << BFE_MDIO_OP_SHIFT) |
				(sc->bfe_phyaddr << BFE_MDIO_PMD_SHIFT) |
				(reg << BFE_MDIO_RA_SHIFT) |
				(BFE_MDIO_TA_VALID << BFE_MDIO_TA_SHIFT) |
				(val & BFE_MDIO_DATA_DATA)));
	status = bfe_wait_bit(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII, 100, 0);
	BFE_UNLOCK(sc);

	return status;
}

/* 
 * XXX - I think this is handled by the PHY driver, but it can't hurt to do it
 * twice
 */
__private_extern__ int
bfe_setupphy( struct bfe_softc * sc )
{
	u_int32_t val;
	BFE_LOCK(sc);

	/* Enable activity LED */
	bfe_readphy(sc, 26, &val);
	bfe_writephy(sc, 26, val & 0x7fff); 
	bfe_readphy(sc, 26, &val);

	/* Enable traffic meter LED mode */
	bfe_readphy(sc, 27, &val);
	bfe_writephy(sc, 27, val | (1 << 6));

	BFE_UNLOCK(sc);
	return 0;
}

__private_extern__ void
bfe_chip_enable( struct bfe_softc * sc )
{
	/* Enable the chip and core */
	BFE_OR(sc, BFE_ENET_CTRL, BFE_ENET_ENABLE);
}

__private_extern__ void
bfe_enable_interrupts( struct bfe_softc * sc )
{
	CSR_WRITE_4(sc, BFE_IMASK, BFE_IMASK_DEF);
}

__private_extern__ void
bfe_disable_interrupts( struct bfe_softc * sc )
{
	CSR_WRITE_4(sc, BFE_IMASK, 0);
	CSR_READ_4(sc, BFE_IMASK);
}
