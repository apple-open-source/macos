/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/pmax/pmax_pci.c,v 1.7 2002/08/27 22:07:08 tsi Exp $ */
/*
 * Copyright 1998 by Concurrent Computer Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Concurrent Computer
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Concurrent Computer Corporation makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * CONCURRENT COMPUTER CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CONCURRENT COMPUTER CORPORATION BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Copyright 1998 by Metro Link Incorporated
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Metro Link
 * Incorporated not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Metro Link Incorporated makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * METRO LINK INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL METRO LINK INCORPORATED BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <stdio.h>
#include "os.h"
#include "compiler.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "Pci.h"

#include <sys/prosrfs.h>
#include <sys/cpu.h>

/*
 * Night Hawk 6400/6408 platform support
 */
#undef NH640X_PCI_MFDEV_SUPPORT
#undef NH640X_PCI_BRIDGE_SUPPORT

static void    nh640xPciInit(void);
static PCITAG  nh640xPciFindNext(void);
static PCITAG  nh640xPciFindFirst(void);
static CARD32  nh6400PciReadLong(PCITAG tag, int offset);
static void    nh6400PciWriteLong(PCITAG tag, int offset, CARD32 val);
static ADDRESS nh6400BusToHostAddr(PCITAG tag, ADDRESS addr);
static ADDRESS nh6400HostToBusAddr(PCITAG tag, ADDRESS addr);
static CARD32  nh6408PciReadLong(PCITAG tag, int offset);
static void    nh6408PciWriteLong(PCITAG tag, int offset, CARD32 val);
static ADDRESS nh6408BusToHostAddr(PCITAG tag, ADDRESS addr);
static ADDRESS nh6408HostToBusAddr(PCITAG tag, ADDRESS addr);

static pciBusFuncs_t nh6400_pci_funcs = {
  nh6400PciReadLong,
  nh6400PciWriteLong,
  nh6400HostToBusAddr,
  nh6400BusToHostAddr
};

static pciBusFuncs_t nh6408_pci_funcs = {
  nh6408PciReadLong,
  nh6408PciWriteLong,
  nh6408HostToBusAddr,
  nh6408BusToHostAddr
};

/*
 * NH640x CFG address and data register offsets from base
 */
#define NH6400_PCI_CFG_ADDR_REG_OFF         0
#define NH6400_PCI_CFG_TYPE0_DATA_REG_OFF   0x40
#define NH6400_PCI_CFG_TYPE1_DATA_REG_OFF   0x80

#define NH6408_PCI_CFG_ADDR_REG_OFF   0
#define NH6408_PCI_CFG_DATA_REG_OFF   0x10000

/*
 * Possible cfg addr values for NH640x GMEM PMC ports
 */
static unsigned long nh6400_pmc_cfgaddrs[] = {
	PCI_CFGMECH1_TYPE0_CFGADDR(0,0,0)
};

/*
 * Possible cfg addr values for devices on a secondary bus
 * (e.g. behind DEC 21152 PCI-to-PCI bridge)
 */
static unsigned long dec_cfgaddrs[] = {
	PCI_CFGMECH1_TYPE1_CFGADDR(1,0,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,1,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,2,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,3,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,4,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,5,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,6,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,7,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,8,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,9,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,10,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,11,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,12,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,13,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,14,0,0),
	PCI_CFGMECH1_TYPE1_CFGADDR(1,15,0,0)
};

/*
 * Data structure holding information about various nh640x PCI buses
 */
struct nh640x_pci_info {
  int            busnum;
  int            type;
  unsigned long  num_cfg_addrs;
  unsigned long *cfg_addrs;
  int            primary_bus;
  unsigned long  cfgPhysBase;
  unsigned long  memBase;
  unsigned long  ioBase;
  unsigned long  ioSize;
  unsigned char *cfgAddrReg;  /* After mapping */
};

/* Type */
#define PRIMARY_PCI   0
#define SECONDARY_PCI 1

static struct nh640x_pci_info nh6400_pci_info[] = {
/* pci4 */  { 4,  PRIMARY_PCI,   1,  nh6400_pmc_cfgaddrs, 0, 0xa0000000, 0xa1000000, 0, 0xa2000000 },
/* pci12 */ { 12, SECONDARY_PCI, 16, dec_cfgaddrs,        4                         },
#if 0
/* pci5 */  { 5,  PRIMARY_PCI,   1,  nh6400_pmc_cfgaddrs, 0, 0xb0000000, 0xb1000000, 0, 0xb2000000 },
/* pci13 */ { 13, SECONDARY_PCI, 16, dec_cfgaddrs,        5                         },
#endif		    
};

#define NH6400_NUM_PCI_EXPANSION_BUSES (sizeof(nh6400_pci_info)/sizeof(struct nh640x_pci_info))

static struct nh640x_pci_info nh6408_pci_info[] = {
/* pci8 */  { 8,  PRIMARY_PCI,   1,  nh6400_pmc_cfgaddrs, 0, 0x98040000, 0x9a800000, 65536, 0xa0000000 },
/* pci12 */ { 12, SECONDARY_PCI, 16, dec_cfgaddrs,        8,                        },
#if 0
/* pci9 */  { 9,  PRIMARY_PCI,   1,  nh6400_pmc_cfgaddrs, 0, 0x99040000, 0x9b800000, 65536, 0xb0000000 },
/* pci13 */ { 13, SECONDARY_PCI, 16, dec_cfgaddrs,        9,                        },
#endif		    
};

#define NH6408_NUM_PCI_EXPANSION_BUSES (sizeof(nh6408_pci_info)/sizeof(struct nh640x_pci_info))

extern unsigned long pmax_sys_type;

#define MOTOPPC_IO_BASE 0x80000000L     /* Start of PCI/ISA I/O region */

extern void * pmax_iomap(unsigned long, unsigned long);
extern unsigned long ioSize;
extern volatile unsigned char *ioBase;

void
pmaxPciInit(void)
{
  extern void motoppcPciInit(void);
  extern void nh640xPciInit(void);
  extern void nh6800tPciInit(void);

  extern unsigned long motoPciMemBase;
  extern unsigned long motoPciMemLen;
  extern unsigned long motoPciMemBaseCPU;
	
  /*
   * Determine type of machine
   */
  switch(pmax_sys_type) {
  case MODEL_NH6400:
  case MODEL_NH6408:
    nh640xPciInit();
    break;

  case MODEL_NH6800T:
    nh6800tPciInit();
    break;
	  
  case MODEL_PH620:
  case MODEL_PH640:
  case MODEL_MMTX:
	motoPciMemBase    = 0;
	motoPciMemLen     = 0x20000000;
	motoPciMemBaseCPU = 0xa0000000;
	/*FALLTHROUGH*/
	
  case MODEL_MPWR:
  case MODEL_PH610:
  case MODEL_MPWR2:
    motoppcPciInit();
    break;

  default:
    FatalError("pmaxPciInit: Unsupported machine type\n");
    break;
  }
}	

void
ppcPciIoMap(int pcibus)
{
	int primary_bus;
	
	if (ioBase != MAP_FAILED)
		munmap((void*)ioBase,ioSize);

	if (!pciBusInfo[pcibus])
		return;
	
	primary_bus = pciBusInfo[pcibus]->primary_bus;

	if (!pciBusInfo[primary_bus])
		return;
	
	ioSize = min(pciBusInfo[primary_bus]->ppc_io_size, 64 * 1024);
	if (ioSize) {
		ioBase = (unsigned char *)pmax_iomap(pciBusInfo[primary_bus]->ppc_io_base, ioSize);
		if (ioBase == MAP_FAILED)
			ioSize = 0;
	}
}

static void
nh640xPciInit(void)
{
  int                     i,n;
  struct nh640x_pci_info *infop;
  pciBusFuncs_p           functions;
	
  switch (pmax_sys_type) {
  case MODEL_NH6400:
	n         = NH6400_NUM_PCI_EXPANSION_BUSES;
	infop     = nh6400_pci_info;
	functions = &nh6400_pci_funcs; 
	break;
  case MODEL_NH6408:
	n         = NH6408_NUM_PCI_EXPANSION_BUSES;
	infop     = nh6408_pci_info;
	functions = &nh6408_pci_funcs; 
	break;
  default:
	FatalError("Unknown Power MAXION system type\n");
	/*NOTREACHED*/
  }

  /*
   * Initialize entries in pciBusInfo[] table for each defined PCI bus.
   * This table is actually sparse because undefined or inaccessible
   * pci buses are left as NULL entries. Of course, pciFindNext() is
   * aware of this convention, and will skip the undefined buses.
   */
  for (i=0; i<n; infop++,i++) {
	  int           bus = infop->busnum;
	  pciBusInfo_t *busp;

	  if (pciBusInfo[bus])
		  busp = pciBusInfo[bus];
	  else
		  busp = xalloc(sizeof(pciBusInfo_t));
	  
	  if (!busp)
		  FatalError("nh640xPciInit: xalloc failed\n");

	  /* Initialize pci bus info */
	  busp->configMech  = PCI_CFG_MECH_OTHER;
	  busp->numDevices  = infop->num_cfg_addrs;
	  busp->secondary   = (infop->type == SECONDARY_PCI ? TRUE : FALSE);
	  busp->primary_bus = infop->primary_bus;
	  busp->funcs       = functions;
	  busp->pciBusPriv  = infop;

	  /* Initialize I/O base/size info */ 
	  if (busp->secondary) {
		  pciBusInfo_t *pri_busp = pciBusInfo[busp->primary_bus];
		  if (pri_busp) {
			  busp->ppc_io_base     = pri_busp->ppc_io_base;
			  busp->ppc_io_size     = pri_busp->ppc_io_size;
		  }
	  }
	  else if (infop->ioSize) {
		  busp->ppc_io_size     = infop->ioSize;
		  busp->ppc_io_base     = infop->ioBase;
	  }
	  
	  pciBusInfo[bus] = busp;
	  
	  /*
	   * Adjust pciNumBuses to reflect the highest defined entry in pciBusInfo
	   */
	  if (pciNumBuses < bus)
		  pciNumBuses = bus + 1;
  }
  
  pciFindFirstFP = nh640xPciFindFirst;
  pciFindNextFP  = nh640xPciFindNext;
}

static PCITAG
nh640xPciFindNext(void)
{
  unsigned long devid, tmp;
  unsigned char base_class, sub_class, sec_bus, pri_bus;
  
  for (;;) {

    if (pciBusNum == -1) {
	    /*
	     * Start at top of the order
	     */
	    pciBusNum = 0;
	    pciFuncNum = 0;
	    pciDevNum = 0;
    }
    else {
#ifdef NH640X_PCI_MFDEV_SUPPORT
	    /*
	     * Somewhere in middle of order.  Determine who's
	     * next up
	     */
	    if (pciFuncNum == 0) {
		    /*
		     * Is current dev a multifunction device?
		     */
		    if (pciMfDev(pciBusNum, pciDevNum))
			    /* Probe for other functions */
			    pciFuncNum = 1;
		    else
			    /* No more functions this device. Next device please */
			    pciDevNum ++;
	    }
	    else if (++pciFuncNum >= 8) {
		    /* No more functions for this device. Next device please */
		    pciFuncNum = 0;
		    pciDevNum ++;
	    }
#else /* NH640X_PCI_MFDEV_SUPPORT */
	    pciDevNum++;
#endif /* NH640X_PCI_MFDEV_SUPPORT */
	    
	    if (!pciBusInfo[pciBusNum] || pciDevNum >= pciBusInfo[pciBusNum]->numDevices) {
		    /*
		     * No more devices for this bus. Next bus please
		     */
		    if (++pciBusNum >= pciNumBuses)
			    /* No more buses.  All done for now */
			    return(PCI_NOT_FOUND);
		    
		    pciDevNum = 0;
	    }
    }

    if (!pciBusInfo[pciBusNum])
	    continue;  /* Undefined bus, next bus/device please */
    
    /*
     * At this point, pciBusNum, pciDevNum, and pciFuncNum have been
     * advanced to the next device.  Compute the tag, and read the
     * device/vendor ID field.
     */
    pciDeviceTag = PCI_MAKE_TAG(pciBusNum, pciDevNum, pciFuncNum);
    devid = pciReadLong(pciDeviceTag, 0);
    if (devid == 0xffffffff)
	    continue; /* Nobody home.  Next device please */

#ifdef NH640X_PCI_BRIDGE_SUPPORT
    /*
     * Before checking for a specific devid, look for enabled
     * PCI to PCI bridge devices.  If one is found, create and
     * initialize a bus info record (if one does not already exist).
     */
    tmp = pciReadLong(pciDeviceTag, PCI_CLASS_CODE_REG);
    base_class = PCI_EXTRACT_BASE_CLASS(tmp);
    sub_class = PCI_EXTRACT_SUBCLASS(tmp); 
    if (base_class == PCI_CLASS_BRIDGE && sub_class == PCI_SUBCLASS_BRIDGE_PCI) {
	    tmp = pciReadLong(pciDeviceTag, PCI_BRIDGE_BUS_REG);
	    sec_bus = PCI_SECONDARY_BUS_EXTRACT(tmp);
	    pri_bus = PCI_PRIMARY_BUS_EXTRACT(tmp);
	    if (sec_bus > 0 && sec_bus < PCI_MAX_BUSES && pcibusInfo[pri_bus]) {
		    /*
		     * Found a secondary PCI bus
		     */
		    if (!pciBusInfo[sec_bus]) {
			    pciBusInfo[sec_bus] = xalloc(sizeof(pciBusInfo_t));

			    if (!pciBusInfo[sec_bus])
				    FatalError("nh640xPciFindNext: alloc failed\n!!!");
		    }

		    /* Copy parents settings... */
		    *pciBusInfo[sec_bus] = *pcibusInfo[pri_bus];

		    /* ...but not everything same as parent */
		    pciBusInfo[sec_bus]->primary_bus = pri_bus;
		    pciBusInfo[sec_bus]->secondary = TRUE;
		    pciBusInfo[sec_bus]->numDevices = 32;

		    if (pciNumBuses < sec_num)
			    pciNumBuses = sec_num+1;
	    }
    }
#endif /* NH640X_PCI_BRIDGE_SUPPORT */
    
    /*
     * Does this device match the requested devid after
     * applying mask?
     */
    if ((devid & pciDevidMask) == pciDevid) {
	    /* Yes - Return it.  Otherwise, next device */

	    /* However, before returning it, try to map */
	    /* I/O region for this PCI bus              */
	    ppcPciIoMap(PCI_BUS_FROM_TAG(pciDeviceTag));
	    
	    return(pciDeviceTag); /* got a match */
    }

  } /* for */
  
  /*NOTREACHED*/
}

static PCITAG
nh640xPciFindFirst(void)
{
  pciBusNum = -1;
  return(nh640xPciFindNext());
}

static unsigned long
nh6400PciReadLong(PCITAG tag, int offset)
{
  unsigned long           tmp;
  char                   *base;
  int                     devnum, bus, func, data_reg_offset, ndevs;
  unsigned long           cfgaddr;
  pciBusInfo_t           *busp, *pri_busp;
  struct nh640x_pci_info *infop, *pri_infop;

  bus    = PCI_BUS_FROM_TAG(tag);
  devnum = PCI_DEV_FROM_TAG(tag);
  func   = PCI_FUNC_FROM_TAG(tag);

  xf86MsgVerb(3, X_INFO,
	      "nh6400PciReadLong: bus=%d, devnum=%d, func=%d, offset=0x%x\n",
	      bus, devnum, func, offset);

  if (bus >= pciNumBuses ||  !pciBusInfo[bus]) {
	  xf86Msg(X_WARNING, "nh6400PciReadLong: bus pci%d not defined!!!\n",
		  bus);
	  return(0xffffffff);
  }
  
  busp = pciBusInfo[bus];
  infop = (struct nh640x_pci_info *)busp->pciBusPriv;

  if (busp->secondary) {
  	  /*
	   * Secondary PCI bus behind a pci-to-pci bridge
	   */
	  pri_busp        = pciBusInfo[busp->primary_bus];
	  pri_infop       = (struct nh640x_pci_info *)pri_busp->pciBusPriv;
	  ndevs           = 16;
	  data_reg_offset = NH6400_PCI_CFG_TYPE1_DATA_REG_OFF;  /* For Type 1 cfg cycles */

	  if (!pri_busp) {
		  xf86Msg(X_WARNING,
			"nh6400PciReadLong: pci%d's primary parent [pci%d] "
			"is not defined!!!\n", bus, busp->primary_bus);
		  return(0xffffffff);
	  }
  }
  else {
	  pri_busp        = busp;
	  pri_infop       = infop;
	  ndevs           = infop->num_cfg_addrs;
	  data_reg_offset = NH6400_PCI_CFG_TYPE0_DATA_REG_OFF;  /* For Type 0 cfg cycles */
  }

  if (devnum >= ndevs) {
	  xf86Msg(X_WARNING,
		"nh6400PciReadLong: devnum %d out of range for bus pci%d\n",
		devnum, bus);
	  return(0xffffffff);
  }
 
  /*
   * Make sure the cfg address and data registers for this bus are mapped
   * Secondary buses just use the primary parents addreses
   */
  if (!infop->cfgAddrReg) {
	  if (!pri_infop->cfgAddrReg) {
		  pri_infop->cfgAddrReg = pmax_iomap(pri_infop->cfgPhysBase, 0x1000);
		  if (pri_infop->cfgAddrReg == MAP_FAILED) {
			  FatalError("nh6400PciReadLong: Cannot map PCI cfg regs @ 0x%08x\n",
				     pri_infop->cfgPhysBase);
			  /*NOTREACHED*/
		  }
	  }
	  infop->cfgAddrReg  = pri_infop->cfgAddrReg;
	  infop->cfgPhysBase = pri_infop->cfgPhysBase;
  }
  base = infop->cfgAddrReg;

  if (busp->secondary) {
	  /* cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(bus,devnum,func,offset); */
	  cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(1,devnum,func,offset); /* Must use bus=1 for now - glb */
  }
  else {
	  cfgaddr  = infop->cfg_addrs[devnum] + offset;
  }
  
  xf86MsgVerb(X_INFO, 3,
	      "nh6400PciReadLong: Writing cfgaddr=0x%x to 0x%x (phys=0x%x)\n",
	      cfgaddr, base, infop->cfgPhysBase); 

  /* There may not be any OS interaction while interrupts are disabled */
  xf86DisableInterrupts();
  
  *((unsigned long *)(base)) = pciByteSwap(cfgaddr); /* Set cfg address */
  eieio();
  
  if (!badaddr(base + data_reg_offset, 4, 0)) {
	tmp = *((unsigned long *)(base + data_reg_offset));
  	eieio();
  }

  xf86EnableInterrupts();
  
  xf86MsgVerb(X_INFO, 3, "nh6400PciReadLong: Read value=0x%x from 0x%x (phys=0x%x)\n",
	      pciByteSwap(tmp), base + data_reg_offset, infop->cfgPhysBase + data_reg_offset);
  
  return(pciByteSwap(tmp));
}

static void
nh6400PciWriteLong(PCITAG tag, int offset, unsigned long val)
{
  char                   *base;
  int                     devnum, bus, func, data_reg_offset, ndevs;
  unsigned long           cfgaddr;
  pciBusInfo_t           *busp, *pri_busp;
  struct nh640x_pci_info *infop, *pri_infop;

  bus    = PCI_BUS_FROM_TAG(tag);
  devnum = PCI_DEV_FROM_TAG(tag);
  func   = PCI_FUNC_FROM_TAG(tag);

  xf86MsgVerb(X_INFO, 3,
	      "nh6400PciWriteLong: bus=%d, devnum=%d, func=%d, offset=0x%x, "
	      val=0x%x\n", bus, devnum, func, offset, val);

  if (bus >= pciNumBuses || !pciBusInfo[bus]) {
	  xf86Msg(X_WARNING, "nh6400PciWriteLong: bus pci%d not defined!!!\n",
		  bus);
	  return;
  }
  busp = pciBusInfo[bus];
  infop = (struct nh640x_pci_info *)busp->pciBusPriv;

  if (busp->secondary) {
  	  /*
	   * Secondary PCI bus behind a pci-to-pci bridge
	   */
	  pri_busp        = pciBusInfo[busp->primary_bus];
	  pri_infop       = (struct nh640x_pci_info *)pri_busp->pciBusPriv;
	  ndevs           = 16;
	  data_reg_offset = NH6400_PCI_CFG_TYPE1_DATA_REG_OFF;  /* For Type 1 cfg cycles */

	  if (!pri_busp) {
		  xf86Msg(X_WARNING,
			  "nh6400PciWriteLong: pci%d's primary parent [pci%d]"
			  " is not defined!!!\n", bus, busp->primary_bus);
		  return;
	  }
  }
  else {
	  pri_busp        = busp;
	  pri_infop       = infop;
	  ndevs           = infop->num_cfg_addrs;
	  data_reg_offset = NH6400_PCI_CFG_TYPE0_DATA_REG_OFF;  /* For Type 0 cfg cycles */
  }

  if (devnum >= ndevs) {
	  xf86Msg(X_WARNING,
		  "nh6400PciWriteLong: devnum %d out of range for bus pci%d\n",
		  devnum, bus);
	  return;
  }

  /*
   * Make sure the cfg address and data registers for this bus are mapped
   * Secondary buses just use the primary parents addreses
   */
  if (!infop->cfgAddrReg) {
	  if (!pri_infop->cfgAddrReg) {
		  pri_infop->cfgAddrReg = pmax_iomap(pri_infop->cfgPhysBase, 0x1000);
		  if (pri_infop->cfgAddrReg == MAP_FAILED) {
			  FatalError("nh6400PciWriteLong: Cannot map PCI cfg regs @ 0x%08x\n",
				     pri_infop->cfgPhysBase);
			  /*NOTREACHED*/
		  }
	  }
	  infop->cfgAddrReg  = pri_infop->cfgAddrReg;
	  infop->cfgPhysBase = pri_infop->cfgPhysBase;
  }
  base = infop->cfgAddrReg;
 
  if (busp->secondary) {
	  /* cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(bus,devnum,func,offset); */
	  cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(1,devnum,func,offset); /* Must use bus=1 for now - glb */
  }
  else {
	  cfgaddr  = infop->cfg_addrs[devnum] + offset;
  }
  
  xf86MsgVerb(X_INFO, 3,
	      "nh6400PciWriteLong: Writing cfgaddr=0x%x to 0x%x (phys=0x%x)\n",
	      cfgaddr, base, infop->cfgPhysBase); 

  /* There may not be any OS interaction while interrupts are disabled */
  xf86DisableInterrupts();
  
  *((unsigned long *)(base)) = pciByteSwap(cfgaddr); /* Set cfg address */
  eieio();
  
  *((unsigned long *)(base + data_reg_offset)) = pciByteSwap(val);
  eieio();
  
  xf86EnableInterrupts();
  
  xf86MsgVerb(X_INFO, 3,
	      "nh6400PciWriteLong: Wrote value=0x%x to 0x%x (phys=0x%x)\n",
	      val, base + data_reg_offset,
	      infop->cfgPhysBase + data_reg_offset);
}

/*
 * These next two functions are for debugging purposes only because
 * the nh6400 does not translate passed to/from a PCI domain.  However,
 * we do do some bounds checking to make sure things are where they
 * should be.
 */
static ADDRESS
nh6400BusToHostAddr(PCITAG tag, ADDRESS addr)
{
	unsigned long           addr_l = (unsigned long)addr;  
	int                     bus = PCI_BUS_FROM_TAG(tag);
	struct nh640x_pci_info *infop;
	int                     pri_bus;
	unsigned long           membase;

	if (!pciBusInfo[bus])
		FatalError("nh6400BusToHostAddr: pci%d not defined!!\n", bus);

	if (pciBusInfo[bus]->secondary) {
		pri_bus = pciBusInfo[bus]->primary_bus;

		if (!pciBusInfo[pri_bus])
			FatalError("nh6400BusToHostAddr: Primary bus pci%d not defined!!\n", pri_bus);
	}
	else
		pri_bus = bus;

	infop = (struct nh640x_pci_info *)pciBusInfo[pri_bus]->pciBusPriv;
	membase = infop->memBase;
			
	if (addr_l < 0x80000000)
		/*
		 * NH6400 host memory addresses are 0-0x7fffffff
		 */
		return(addr);
	
	else if (addr_l >= membase && addr_l < membase + 0x0e000000)
		/*
		 * NH6400 host can access PCI memory space addresses
		 * [memBase, memBase+0x0dffffff] 
		 */
		return(addr);
	else
		/* Other addresses are not valid */
		FatalError("nh6400BusToHostAddr: Bus address 0x%x not visible to NH6400 host\n",
			   addr_l);
	
	/*NOTREACHED*/
}

static ADDRESS
nh6400HostToBusAddr(PCITAG tag, ADDRESS addr)
{
	unsigned long           addr_l = (unsigned long) addr;
	int                     bus = PCI_BUS_FROM_TAG(tag);
	struct nh640x_pci_info *infop;
	int                     pri_bus;
	unsigned long           membase;

	if (!pciBusInfo[bus])
		FatalError("nh6400HostToBusAddr: pci%d not defined!!\n", bus);

	if (pciBusInfo[bus]->secondary) {
		pri_bus = pciBusInfo[bus]->primary_bus;

		if (!pciBusInfo[pri_bus])
			FatalError("nh6400HostToBusAddr: Primary bus pci%d not defined!!\n", pri_bus);
	}
	else
		pri_bus = bus;

	infop = (struct nh640x_pci_info *)pciBusInfo[pri_bus]->pciBusPriv;
	membase = infop->memBase;
			
	if (addr_l < 0x80000000)
		/*
		 * NH6400 host memory addresses are 0-0x7fffffff
		 */
		return(addr);
	
	else if (addr_l >= membase && addr_l < membase + 0x0e000000)
		/*
		 * NH6400 host can access PCI memory space addresses
		 * [memBase, memBase+0x0dffffff] 
		 */
		return(addr);
	else
		/* Other addresses are not valid */
		FatalError("nh6400HostToBusAddr: Bus address 0x%x not visible to NH6400 host\n",
			   addr_l);
	
	/*NOTREACHED*/
}


/*
 * NH6408 platform support
 */
static unsigned long
nh6408PciReadLong(PCITAG tag, int offset)
{
  unsigned long           tmp;
  char                   *base;
  int                     devnum, bus, func, ndevs;
  unsigned long           cfgaddr;
  pciBusInfo_t           *busp, *pri_busp;
  struct nh640x_pci_info *infop, *pri_infop;

  bus    = PCI_BUS_FROM_TAG(tag);
  devnum = PCI_DEV_FROM_TAG(tag);
  func   = PCI_FUNC_FROM_TAG(tag);

  xf86MsgVerb(X_INFO,
	      "nh6408PciReadLong: bus=%d, devnum=%d, func=%d, offset=0x%x\n",
	      bus, devnum, func, offset);

  if (bus >= pciNumBuses || !pciBusInfo[bus]) {
	  xf86Msg(X_WARNING, "nh6408PciReadLong: bus pci%d not defined!!!\n",
		  bus);
	  return(0xffffffff);
  }
  
  busp = pciBusInfo[bus];
  infop = (struct nh640x_pci_info *)busp->pciBusPriv;

  if (busp->secondary) {
  	  /*
	   * Secondary PCI bus behind a pci-to-pci bridge
	   */
	  pri_busp        = pciBusInfo[busp->primary_bus];
	  pri_infop       = (struct nh640x_pci_info *)pri_busp->pciBusPriv;
	  ndevs           = 16;

	  if (!pri_busp) {
		  xf86Msg(X_WARNING,
			  "nh6408PciReadLong: pci%d's primary parent [pci%d] "
			  "is not defined!!!\n", bus, busp->primary_bus);
		  return(0xffffffff);
	  }
  }
  else {
	  pri_busp        = busp;
	  pri_infop       = infop;
	  ndevs           = infop->num_cfg_addrs;
  }

  if (devnum >= ndevs) {
	  xf86Msg(X_WARNING
		  "nh6408PciReadLong: devnum %d out of range for bus pci%d\n",
		  devnum, bus);
	  return(0xffffffff);
  }

  /*
   * Make sure the cfg address and data registers for this bus are mapped
   * Secondary buses just use the primary parents addreses
   */
  if (!infop->cfgAddrReg) {
	  if (!pri_infop->cfgAddrReg) {
		  pri_infop->cfgAddrReg = pmax_iomap(pri_infop->cfgPhysBase, 0x11000);
		  if (pri_infop->cfgAddrReg == MAP_FAILED) {
			  FatalError("nh6408PciReadLong: Cannot map PCI cfg regs @ 0x%08x\n",
				     pri_infop->cfgPhysBase);
			  /*NOTREACHED*/
		  }
	  }
	  infop->cfgAddrReg  = pri_infop->cfgAddrReg;
	  infop->cfgPhysBase = pri_infop->cfgPhysBase;
  }
  base = infop->cfgAddrReg;
 
  if (busp->secondary) {
	  /* cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(bus,devnum,func,offset); */
	  cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(1,devnum,func,offset); /* Must use bus=1 for now - glb */
  }
  else {
	  cfgaddr  = infop->cfg_addrs[devnum] + offset;
  }
  
  xf86MsgVerb(X_INFO, 3,
	      "nh6408PciReadLong: Writing cfgaddr=0x%x to 0x%x (phys=0x%x)\n",
	      cfgaddr, base, infop->cfgPhysBase); 

  /* There may not be any OS interaction while interrupts are disabled */
  xf86DisableInterrupts();
  
  *((unsigned long *)(base)) = pciByteSwap(cfgaddr); /* Set cfg address */
  eieio();
  
  if (!badaddr(base + NH6408_PCI_CFG_DATA_REG_OFF, 4, 0)) {
    tmp = *((unsigned long *)(base + NH6408_PCI_CFG_DATA_REG_OFF));
    eieio();
  }

  xf86EnableInterrupts();
  
  xf86MsgVerb(X_INFO, 3, "nh6408PciReadLong: Read value=0x%x from 0x%x (phys=0x%x)\n",
	      pciByteSwap(tmp),
	      base + NH6408_PCI_CFG_DATA_REG_OFF,
	      infop->cfgPhysBase + NH6408_PCI_CFG_DATA_REG_OFF);
  
  return(pciByteSwap(tmp));
}

static void
nh6408PciWriteLong(PCITAG tag, int offset, unsigned long val)
{
  char                   *base;
  int                     devnum, bus, func, ndevs;
  unsigned long           cfgaddr;
  pciBusInfo_t           *busp, *pri_busp;
  struct nh640x_pci_info *infop, *pri_infop;

  bus    = PCI_BUS_FROM_TAG(tag);
  devnum = PCI_DEV_FROM_TAG(tag);
  func   = PCI_FUNC_FROM_TAG(tag);

  xf86MsgVerb(X_INFO,
	      "nh6408PciWriteLong: bus=%d, devnum=%d, func=%d, offset=0x%x, "
	      "val=0x%x\n", bus, devnum, func, offset, val);

  if (bus >= pciNumBuses || !pciBusInfo[bus]) {
	  xf86Msg(X_WARNING, "nh6408PciWriteLong: bus pci%d not defined!!!\n",
		  bus);
	  return;
  }
  
  busp = pciBusInfo[bus];
  infop = (struct nh640x_pci_info *)busp->pciBusPriv;
  
  if (busp->secondary) {
  	  /*
	   * Secondary PCI bus behind a pci-to-pci bridge
	   */
	  pri_busp        = pciBusInfo[busp->primary_bus];
	  pri_infop       = (struct nh640x_pci_info *)pri_busp->pciBusPriv;
	  ndevs           = 16;

	  if (!pri_busp) {
		  xf86Msg(X_WARNING,
			  "nh6408PciWriteLong: pci%d's primary parent [pci%d] "
			  is not defined!!!\n", bus, busp->primary_bus);
		  return;
	  }
  }
  else {
	  pri_busp        = busp;
	  pri_infop       = infop;
	  ndevs           = infop->num_cfg_addrs;
  }

  if (devnum >= ndevs) {
	  xf86Msg(X_WARNING,
		  "nh6408PciWriteLong: devnum %d out of range for bus pci%d\n",
		  devnum, bus);
	  return;
  }

  /*
   * Make sure the cfg address and data registers for this bus are mapped
   * Secondary buses just use the primary parents addreses
   */
  if (!infop->cfgAddrReg) {
	  if (!pri_infop->cfgAddrReg) {
		  pri_infop->cfgAddrReg = pmax_iomap(pri_infop->cfgPhysBase, 0x11000);
		  if (pri_infop->cfgAddrReg == MAP_FAILED) {
			  FatalError("nh6408PciWriteLong: Cannot map PCI cfg regs @ 0x%08x\n",
				     pri_infop->cfgPhysBase);
			  /*NOTREACHED*/
		  }
	  }
	  infop->cfgAddrReg  = pri_infop->cfgAddrReg;
	  infop->cfgPhysBase = pri_infop->cfgPhysBase;
  }
  base = infop->cfgAddrReg;
 
  if (busp->secondary) {
	  /* cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(bus,devnum,0,offset); */
	  cfgaddr = PCI_CFGMECH1_TYPE1_CFGADDR(1,devnum,0,offset);
  }
  else {
	  cfgaddr  = infop->cfg_addrs[devnum] + offset;
  }
  
  xf86MsgVerb(X_INFO, 3,
	      "nh6408PciWriteLong: Writing cfgaddr=0x%x to 0x%x (phys=0x%x)\n",
	      cfgaddr, base, infop->cfgPhysBase); 

  /* There may not be any OS interaction while interrupts are disabled */
  xf86DisableInterrupts();
  
  *((unsigned long *)(base)) = pciByteSwap(cfgaddr); /* Set cfg address */
  eieio();
  
  *((unsigned long *)(base + NH6408_PCI_CFG_DATA_REG_OFF)) = pciByteSwap(val);
  eieio();
  
  xf86EnableInterrupts();
  
  xf86MsgVerb(X_INFO, 3,
	      "nh6408PciWriteLong: Wrote value=0x%x to 0x%x (phys=0x%x)\n",
	      val, base + NH6408_PCI_CFG_DATA_REG_OFF,
	      infop->cfgPhysBase + NH6408_PCI_CFG_DATA_REG_OFF);
}


static ADDRESS
nh6408BusToHostAddr(PCITAG tag, ADDRESS addr)
{
	unsigned long           addr_l = (unsigned long)addr;
	int                     bus = PCI_BUS_FROM_TAG(tag);
	int                     pri_bus;
	struct nh640x_pci_info *infop;
	unsigned long           membase;

	if (!pciBusInfo[bus])
		FatalError("nh6408BusToHostAddr: pci%d not defined!!\n", bus);

	if (pciBusInfo[bus]->secondary) {
		pri_bus = pciBusInfo[bus]->primary_bus;

		if (!pciBusInfo[pri_bus])
			FatalError("nh6408BusToHostAddr: Primary bus pci%d not defined!!\n", pri_bus);
	}
	else
		pri_bus = bus;

	infop = (struct nh640x_pci_info *)pciBusInfo[pri_bus]->pciBusPriv;
	membase = infop->memBase;
			
	if (addr_l < 0x10000000)
		/*
		 * NH6408 host sees PCI memory space addresses 0-0x0fffffff
		 * at the primary PCI buses "memBase" [memBase, memBase+0x0fffffff] 
		 */
		return((ADDRESS)(membase + addr_l));
	
	else if (addr_l >= 0x80000000)
		/*
		 * NH6408 host memory addresses 0-0x7fffffff are seen at
		 * 0x80000000-0xffffffff on PCI
		 */
		return((ADDRESS)(addr_l & 0x7fffffff));
	
	else
		/* Other addresses are not valid */
		FatalError("nh6408BusToHostAddr: Bus address 0x%x not visible to NH6408 host\n",
			   addr_l);
	
	/*NOTREACHED*/
}

static ADDRESS
nh6408HostToBusAddr(PCITAG tag, ADDRESS addr)
{
	unsigned long           addr_l = (unsigned long)addr;
	int                     bus = PCI_BUS_FROM_TAG(tag);
	int                     pri_bus;
	struct nh640x_pci_info *infop;
	unsigned long           membase;

	if (!pciBusInfo[bus])
		FatalError("nh6408HostToBusAddr: pci%d not defined!!\n", bus);

	if (pciBusInfo[bus]->secondary) {
		pri_bus = pciBusInfo[bus]->primary_bus;

		if (!pciBusInfo[pri_bus])
			FatalError("nh6408HostToBusAddr: Primary bus pci%d not defined!!\n", pri_bus);
	}
	else
		pri_bus = bus;

	infop = (struct nh640x_pci_info *)pciBusInfo[pri_bus]->pciBusPriv;
	membase = infop->memBase;
			
	if (addr_l < 0x80000000)
		/*
		 * NH6408 host memory addresses 0-0x7fffffff are seen at
		 * 0x80000000-0xffffffff on PCI
		 */
		return((ADDRESS)(addr_l | 0x80000000));

	else if (addr_l >= membase && addr_l < (membase + 0x10000000))
		/*
		 * NH6408 host sees PCI memory space addresses 0-0x0fffffff
		 * at the primary PCI buses "memBase" [memBase, memBase+0x0fffffff] 
		 */
		return((ADDRESS)(addr_l - membase));
	
	else
		/* Other addresses are not valid */
		FatalError("nh6408HostToBusAddr: Host address 0x%x not visible to pci%d\n",
			   addr_l, bus);
	
	/*NOTREACHED*/
}

/*
 * NH6800 (Turbo) support
 */
static void
nh6800tPciInit(void)
{
	FatalError("nh6800tPciInit: NH6800TURBO not supported (yet)!!!\n");
}
