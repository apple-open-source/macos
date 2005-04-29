/*======================================================================

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Contributor:  Apple Computer, Inc.  Portions © 2003 Apple Computer, 
    Inc. All rights reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.

======================================================================*/

// shim code for linux based code

#include <IOKit/pccard/IOPCCard.h>

extern "C" {

int
IOPCCardAddCSCInterruptHandlers(IOPCCardBridge *bus, unsigned int socket, unsigned int irq,
				int (*top_handler)(int), int (*bottom_handler)(int), 
				int (*enable_functional)(int), int (*disable_functional)(int),
				const char* name)
{
    return !bus->addCSCInterruptHandler(socket, irq, top_handler, bottom_handler, 
					enable_functional, disable_functional, name);
}

int
IOPCCardRemoveCSCInterruptHandlers(IOPCCardBridge *bus, unsigned int socket)
{
    return !bus->removeCSCInterruptHandler(socket);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOCardBusDevice *
IOPCCardCreateCardBusNub(IOPCCardBridge *bus, unsigned int socket, unsigned int function)
{
    return bus->createCardBusNub(socket, function);
}

void
IOPCCardRetainNub(IOService *nub)
{
    nub->retain();
}

void
IOPCCardReleaseNub(IOService *nub)
{
    nub->release();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int
IOPCCardReadConfigByte(IOPCIDevice *bus, unsigned char reg, unsigned char *val)
{
    *val = bus->configRead8(reg);
    return 0;
}

int
IOPCCardWriteConfigByte(IOPCIDevice *bus, unsigned char reg, unsigned char val)
{
    bus->configWrite8(reg, val);
    return 0;
}

int
IOPCCardReadConfigWord(IOPCIDevice *bus, unsigned char reg, unsigned short *val)
{
    *val = bus->configRead16(reg);
    return 0;
}

int 
IOPCCardWriteConfigWord(IOPCIDevice *bus, unsigned char reg, unsigned short val)
{
    bus->configWrite16(reg, val);
    return 0;
}

int 
IOPCCardReadConfigLong(IOPCIDevice *bus, unsigned char reg, unsigned int *val)
{
    *val = bus->configRead32(reg);
    return 0;
}

int
IOPCCardWriteConfigLong(IOPCIDevice *bus, unsigned char reg, unsigned int val)
{
    bus->configWrite32(reg, val);
    return 0;
}

unsigned char
IOPCCardReadByte(void * virt)
{
    UInt8 value;

    value = *(volatile UInt8 *)virt;
    OSSynchronizeIO();

    return value;
}

unsigned long
IOPCCardReadLong(void * virt)
{
    UInt32 value;

    value = OSReadLittleInt32((volatile void *)virt, 0);
    OSSynchronizeIO();

    return value;
}

void
IOPCCardWriteByte(void * virt, unsigned char value)
{
    *(volatile UInt8 *)virt = value;
    OSSynchronizeIO();
}

void
IOPCCardWriteLong(void * virt, unsigned int value)
{
    OSWriteLittleInt32((volatile void *)virt, 0, value);
    OSSynchronizeIO();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern OSSet* gIOPCCardMappedBridgeSpace;

void *
IOPCCardIORemap(u_long paddr, u_long size)
{
    IODeviceMemory *range = IODeviceMemory::withRange(paddr, size);
    if (!range) return 0;
    IOMemoryMap *map = range->map();
    if (!map) {
        range->release();
        return 0;
    }

//  IOLog("ioremap: mapping 0x%x size 0x%x to 0x%x\n", paddr, size, map->getVirtualAddress());

    if (!gIOPCCardMappedBridgeSpace->setObject(map)) {
        IOLog("IOPCCardIORemap(): addition to gIOPCCardMappedBridgeSpace failed\n");
    }

    return (void *)map->getVirtualAddress();
}

void
IOPCCardIOUnmap(void *vaddr)
{
//  IOLog("iounmap: unmapping virtual address 0x%x\n", vaddr);

    IOMemoryMap *map;
    OSIterator *iter = OSCollectionIterator::withCollection(gIOPCCardMappedBridgeSpace);
    if (iter) {
        while ((map = (IOMemoryMap *)iter->getNextObject())) {
            if ((void *)map->getVirtualAddress() == vaddr) {
		IODeviceMemory *range = (IODeviceMemory *)map->getMemoryDescriptor();
		map->release();
		range->release();
		iter->release();
		return;
	    }
	}
        iter->release();
    }
    IOLog("IOPCCardIOUnmap(): failed to unmap virtual address %p\n", vaddr);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
timerFunnel(void *arg0, void *arg1)
{
    gIOPCCardWorkLoop->runAction((IOWorkLoop::Action)gCardServicesGate, NULL,
				 (void *)kCSGateTimerCallout, arg0, NULL, NULL);
}

void
IOPCCardAddTimer(struct timer_list * timer)
{
    AbsoluteTime                    deadline;

    clock_interval_to_deadline(timer->expires, NSEC_PER_SEC / HZ, &deadline);
    thread_call_func_delayed(timerFunnel, (void *)timer, deadline);
}

int
IOPCCardDeleteTimer(struct timer_list * timer)
{
    return (int)thread_call_func_cancel(timerFunnel, (void *)timer, FALSE);
}

} /* extern c */



