/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1994 NeXT Computer, Inc.
 *
 * PCI bus probing.
 *
 * HISTORY
 *
 * 03 May 1994 Dean Reece at NeXT
 *	Created
 */

#include <mach/mach_types.h>
// #include "legacy/KernDevice.h"
#include "legacy/PCI.h"

#include "kernBootStruct.h"
#include "libsaio.h"
#include "io_inline.h"
#include "pci.h"

static BOOL testMethod1(void);
static BOOL testMethod2(void);
typedef unsigned long (*_pci_bus_method_t)(
    unsigned char addr,
    unsigned char dev,
    unsigned char func,
    unsigned char bus
);
static unsigned long getMethod1(
    unsigned char addr,
    unsigned char dev,
    unsigned char func,
    unsigned char bus
);
static unsigned long getMethod2(
    unsigned char addr,
    unsigned char dev,
    unsigned char func,
    unsigned char bus
);
static unsigned
scanBus(
    unsigned maxBusNum,
    unsigned maxDevNum,
    _pci_bus_method_t method,
    _pci_slot_info_t *slot_array
);

_pci_slot_info_t *PCISlotInfo;

_pci_slot_info_t *
PCI_Bus_Init(
    PCI_bus_info_t *info	/* pass in the PCI boot info struct */
)
{

    unsigned int maxBusNum = 0, maxDevNum = 0, useMethod = 0;
    _pci_bus_method_t method = NULL;
    _pci_slot_info_t *slot_array;
    unsigned nslots;

    maxBusNum = info->maxBusNum;
    if (info->BIOSPresent) {
	if (info->u_bus.s.configMethod1) {
	    useMethod=1;
	    maxDevNum=31;
	}
	else if (info->u_bus.s.configMethod2) {
	    useMethod=2;
	    maxDevNum=15;
	}
    }
    else {
	if (testMethod1()) {
	    useMethod=1;
	    maxDevNum=31;
	}
	else if (testMethod2()) {
	    useMethod=2;
	    maxDevNum=15;
	}
    }


    if (useMethod == 1)
	method = getMethod1;
    else if (useMethod == 2)
	method = getMethod2;
    else
	return NULL;
	
    nslots = scanBus(maxBusNum, maxDevNum, method, NULL);
    slot_array = (_pci_slot_info_t *)
	malloc(sizeof(_pci_slot_info_t) * nslots +1);
    (void)scanBus(maxBusNum, maxDevNum, method, slot_array);
    slot_array[nslots].pid = 0x00;
    slot_array[nslots].sid = 0x00;
    PCISlotInfo = slot_array;
    return slot_array;
}

static unsigned
scanBus(
    unsigned maxBusNum,
    unsigned maxDevNum,
    _pci_bus_method_t method,
    _pci_slot_info_t *slot_array
)
{
    unsigned int bus, dev, func;
    unsigned int pid=0, sid=0;
    unsigned count = 0;

    for (bus=0; bus<=maxBusNum; bus++) {
	for (dev=0; dev<=maxDevNum; dev++) {
	    for (func=0; func<8; func++) {
		pid = (*method)(0x00, dev, func, bus);
		if ( ((pid&0xffff) != 0xffff) &&
			((pid&0xffff) != 0x0000) ) {
    
		    /* device found, do whatever here */
		    count++;
		    if (slot_array) {
			sid = (*method)(0x2c, dev, func, bus);
			slot_array->pid = pid;
			slot_array->sid = sid;
			slot_array->dev = dev;
			slot_array->func = func;
			slot_array->bus = bus;
			slot_array++;
#if DEBUG
printf ("{\"0x%08x:%08x\" = {\"Location\" = \"Dev:%d Func:%d Bus:%d\"; }\n",
pid, sid, dev, func, bus);
#endif
		    }
    
		    /* skip 1..7 if not multi-func device */
		    pid = (*method)(12, dev, func, bus);
		    if ((pid & 0x00800000)==0) break;
		}
	    }
	}
    }
    return count;
}

/* defines for Configuration Method #1 (PCI 2.0 Spec, sec 3.6.4.1.1) */
#define PCI_CONFIG_ADDRESS      0x0cf8
#define PCI_CONFIG_DATA         0x0cfc

static BOOL
testMethod1(void)
{
    unsigned long address, data;

    for (address = 0x80000000; address < 0x80010000; address += 0x800) {
	outl (PCI_CONFIG_ADDRESS, address);
	if (inl (PCI_CONFIG_ADDRESS) != address) {
	    return NO;
	}
	data = inl(PCI_CONFIG_DATA);
	if ((data != PCI_DEFAULT_DATA) && (data != 0x00)) {
	    outl (PCI_CONFIG_ADDRESS, 0);
	    return YES;
	}
    }

    outl (PCI_CONFIG_ADDRESS, 0);
    return NO;
}

static unsigned long
getMethod1(
    unsigned char addr,
    unsigned char dev,
    unsigned char func,
    unsigned char bus
)
{
    unsigned long ret=PCI_DEFAULT_DATA;

    union {
	unsigned long d;
	struct {
	    unsigned long	addr:8;
	    unsigned long	func:3;
	    unsigned long	dev:5;
	    unsigned long	bus:8;
	    unsigned long	key:8;
	} s;
    } address;

    address.d = 0x00000000;
    address.s.key=0x80;
    address.s.addr=addr;
    address.s.dev=dev;
    address.s.func=func;
    address.s.bus=bus;
    outl (PCI_CONFIG_ADDRESS, address.d);

    if (inl (PCI_CONFIG_ADDRESS) == address.d)
	ret = inl(PCI_CONFIG_DATA);

    outl (PCI_CONFIG_ADDRESS, 0);
    return ret;
}


/* defines for Configuration Method #2 (PCI 2.0 Spec, sec 3.6.4.1.3) */
#define PCI_CSE_REGISTER	0x0cf8
#define PCI_BUS_FORWARD		0x0cfa

static BOOL
testMethod2(void)
{
    unsigned long address, data;

    /*  Enable configuration space at I/O ports Cxxx.  */

    outb (PCI_CSE_REGISTER, 0xF0);
    if (inb (PCI_CSE_REGISTER) != 0xF0) {
	return NO;
    }

    outb (PCI_BUS_FORWARD, 0x00);
    if (inb (PCI_BUS_FORWARD) != 0x00) {
	return NO;
    }
    /*  Search all devices on the bus.  */
    for (address = 0xc000; address <= 0xcfff; address += 0x100) {
	data = inl(address);
	if ((data != PCI_DEFAULT_DATA) && (data != 0x00)) {
	    outb (PCI_CSE_REGISTER, 0);
	    return YES;
	}
    }

    outb (PCI_CSE_REGISTER, 0);
    return NO;
}

static unsigned long
getMethod2(
    unsigned char addr,
    unsigned char dev,
    unsigned char func,
    unsigned char bus
)
{
    unsigned long ret=PCI_DEFAULT_DATA;
    unsigned short address;
    unsigned char cse;

    cse = 0xf0 | (func<<1);
    outb(PCI_CSE_REGISTER, cse);
    if (inb(PCI_CSE_REGISTER) == cse) {
	outb(PCI_BUS_FORWARD, bus);
	    if (inb(PCI_BUS_FORWARD) == bus) {
	    address = 0xc000 | (addr&0xfc) | ((dev&0x0f)<<8);
	    ret = inl(address);
	}
	outb(PCI_BUS_FORWARD, 0x00);
    }
    outb(PCI_CSE_REGISTER, 0x00);

    return ret;
}

