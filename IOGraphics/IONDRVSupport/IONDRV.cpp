/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSContainers.h>

extern "C"
{
#include <pexpert/pexpert.h>
#include <kern/host.h>
#include <kern/simple_lock.h>
    extern simple_lock_data_t kmod_lock;
    extern kmod_info_t *kmod;
};

#include "IONDRV.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndAbstractStructors(IONDRV, OSObject)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if __ppc__

#include "IOPEFLoader.h"

#define LOG		if(1) kprintf

static IOLock  * gIOPEFLock;
static OSArray * gIOPEFContainers;

#define super IONDRV
OSDefineMetaClassAndStructorsWithInit(IOPEFNDRV, IONDRV, IOPEFNDRV::initialize());

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOPEFNDRV::initialize( void )
{
    if (!gIOPEFLock)
	gIOPEFLock = IOLockAlloc();
    if (!gIOPEFContainers)
	gIOPEFContainers = OSArray::withCapacity(2);
}

IONDRV * IOPEFNDRV::instantiate( IORegistryEntry * regEntry,
                                 IOLogicalAddress container,
                                 IOByteCount containerSize,
                                 IONDRVUndefinedSymbolHandler undefHandler,
                                 void * self )
{
    OSStatus	err = 1;
    IOPEFNDRV *	inst;
    char * 	name;
    IOByteCount	plen;
    char	kmodName[KMOD_MAX_NAME * 2];
    char	kmodVers[KMOD_MAX_NAME];

    inst = new IOPEFNDRV;

    if (inst)
        do
        {
            if (false == inst->init())
                continue;

            err = PCodeOpen( (void *)container, containerSize, &inst->fPEFInst );
            if (err)
                continue;

            err = PCodeInstantiate( inst->fPEFInst, undefHandler, self );
            if (err)
                continue;

            inst->getSymbol( "DoDriverIO", (IOLogicalAddress *) &inst->fDoDriverIO );
            if (kIOReturnSuccess == inst->getSymbol("TheDriverDescription",
                                                    (IOLogicalAddress *) &inst->fDriverDesc))
            {
                name = (char *) inst->fDriverDesc->driverOSRuntimeInfo.driverName;
                plen = name[ 0 ];
                if (plen >= sizeof(inst->fDriverDesc->driverOSRuntimeInfo.driverName))
                    plen = sizeof(inst->fDriverDesc->driverOSRuntimeInfo.driverName) - 1;
                strncpy( inst->fName, name + 1, plen);
                sprintf( inst->fName + plen, "-%08lx", *((UInt32 *) &inst->fDriverDesc->driverType.version));
            }

            name = (char *) inst->fDriverDesc->driverType.nameInfoStr;
            plen = name[ 0 ];
            if (plen >= sizeof(inst->fDriverDesc->driverType.nameInfoStr))
                plen = sizeof(inst->fDriverDesc->driverType.nameInfoStr) - 1;

            strcpy( kmodName, "com.apple.driver.ndrv.");
            strncat( kmodName, name + 1, plen);
            sprintf( kmodVers, ".0x%lx", inst->fDoDriverIO
					    ? (UInt32) inst->fDoDriverIO->pc
					    : (UInt32) container);
            strcat( kmodName, kmodVers);

            {
#define DEVELOPMENT_STAGE 0x20
#define ALPHA_STAGE 0x40
#define BETA_STAGE 0x60
#define RELEASE_STAGE 0x80
                UInt32 vers = *((UInt32 *) &inst->fDriverDesc->driverType.version);
                UInt8  major1, major2, minor1, minor2, build;
                char * s = kmodVers;
                char   c;

                major1 = (vers & 0xF0000000) >> 28;
                major2 = (vers & 0x0F000000) >> 24;
                minor1 = (vers & 0x00F00000) >> 20;
                minor2 = (vers & 0x000F0000) >> 16;
                build  = (vers & 0x000000FF);

                switch ((vers & 0x0000FF00) >> 8)
                {
                    case RELEASE_STAGE:
                        c = 'f';
                        break;
                    case DEVELOPMENT_STAGE:
                        c = 'd';
                        break;
                    case ALPHA_STAGE:
                        c = 'd';
                        break;
                    default:
                    case BETA_STAGE:
                        c = 'b';
                        break;
                }

                if (major1 > 0)
                    s += sprintf(s, "%d", major1);
		s += sprintf(s, "%d.%d.%d", major2, minor1, minor2);
		if (build)
		    s += sprintf(s, "%c%d", c, build);
            }

            if (KERN_SUCCESS == kmod_create_fake(kmodName, kmodVers))
            {
                simple_lock(&kmod_lock);
                inst->fKModInfo = kmod_lookupbyname( kmodName );
                simple_unlock(&kmod_lock);

                if (inst->fKModInfo)
                {
                    inst->fKModInfo->address = round_page_32((vm_address_t) container);
                    inst->fKModInfo->size    = trunc_page_32((vm_size_t) container + containerSize)
                                             - inst->fKModInfo->address;
                }
            }
        }
        while (false);

    if (inst && err)
    {
        inst->release();
        inst = 0;
    }

    return (inst);
}

void IOPEFNDRV::free( void )
{
    kmod_info_t *k;
    kmod_info_t *p;

    if (fKModInfo)
    {
        simple_lock(&kmod_lock);
        k = p = kmod;
        while (k)
        {
            if (k == fKModInfo)
            {
                if (k == p)
                {    // first element
                    kmod = k->next;
                }
                else
                {
                    p->next = k->next;
                }
            }
            p = k;
            k = k->next;
        }
        simple_unlock(&kmod_lock);

        kfree( (vm_address_t) fKModInfo, sizeof(kmod_info_t) );
    }

    if (fPEFInst)
        PCodeClose( fPEFInst );

    super::free();
}

IOReturn IOPEFNDRV::getSymbol( const char * symbolName,
                               IOLogicalAddress * address )
{
    OSStatus            err;

    err = PCodeFindExport( fPEFInst, symbolName,
                           (LogicalAddress *)address, NULL );
    if (err)
        *address = 0;

    return (err);
}

extern "C" IOReturn _IONDRVLibrariesInitialize( IOService * provider );
// osfmk/ppc/mappings.h
extern "C" void ignore_zero_fault(boolean_t);

IOReturn IOPEFNDRV::doDriverIO( UInt32 commandID, void * contents,
                                UInt32 commandCode, UInt32 commandKind )
{
    OSStatus			err = kIOReturnSuccess;
    struct DriverInitInfo	initInfo;
    CntrlParam *         	pb;

    if (0 == fDoDriverIO)
        return (kIOReturnUnsupported);

    switch (commandCode)
    {
        case kIONDRVReplaceCommand:
        case kIONDRVInitializeCommand:
            err = _IONDRVLibrariesInitialize( (IOService *) contents );
            if (kIOReturnSuccess != err)
                break;
            /* fall thru */

        case kIONDRVFinalizeCommand:
        case kIONDRVSupersededCommand:
            initInfo.refNum = 0xffff & ((UInt32) this);
            MAKE_REG_ENTRY((&initInfo.deviceEntry), contents)
            contents = &initInfo;
            break;

        case kIONDRVControlCommand:
        case kIONDRVStatusCommand:
        case kIONDRVOpenCommand:
        case kIONDRVCloseCommand:
        case kIONDRVReadCommand:
        case kIONDRVWriteCommand:
        case kIONDRVKillIOCommand:

            pb = (CntrlParam *) contents;
            pb->qLink = 0;
            pb->ioCRefNum = 0xffff & ((UInt32) this);
            break;
    }

    if (kIOReturnSuccess == err)
    {
        commandCode -= kIONDRVOpenCommand - kOpenCommand;

        ignore_zero_fault( true );
        err = CallTVector( /*AddressSpaceID*/ 0, (void *)commandID, contents,
                                              (void *)commandCode, (void *)commandKind, /*p6*/ 0,
                                              fDoDriverIO );
        ignore_zero_fault( false );
    }

    return (err);
}

IONDRV * IOPEFNDRV::fromRegistryEntry( IORegistryEntry * regEntry,
				       OSData * newData,
                                       IONDRVUndefinedSymbolHandler handler,
                                       void * self )
{
    IOLogicalAddress	pef = 0;
    IOByteCount		propSize = 0;
    OSData *		prop;
    IONDRV *		inst;
    unsigned int 	i;

    if (newData)
    {
	regEntry->removeProperty("AAPL,ndrvInst");

	prop = (OSData *) regEntry->copyProperty("driver,AAPL,MacOS,PowerPC");
	if (prop)
	{
	    IOLockLock(gIOPEFLock);
	    i = gIOPEFContainers->getNextIndexOfObject(prop, 0);
	    if (i != (unsigned int) -1)
		gIOPEFContainers->removeObject(i);
	    IOLockUnlock(gIOPEFLock);
	
	    prop->release();
	}
	prop = newData;
    }
    else
    {
	inst = (IONDRV *) regEntry->copyProperty("AAPL,ndrvInst");
	if (inst)
	    return (inst);
	prop = (OSData *) regEntry->getProperty("driver,AAPL,MacOS,PowerPC");
    }

    if (prop)
    {
	IOLockLock(gIOPEFLock);
	for (i = 0; (newData = (OSData *) gIOPEFContainers->getObject(i)); i++)
	{
	    if (true && prop->isEqualTo(newData))
	    {
		prop = newData;
		break;
	    }
	}
	if (!newData)
	    gIOPEFContainers->setObject(prop);
	IOLockUnlock(gIOPEFLock);

	regEntry->setProperty("driver,AAPL,MacOS,PowerPC", prop);
        pef = (IOLogicalAddress) prop->getBytesNoCopy();
        propSize = prop->getLength();
    }

    if (pef)
    {
        kprintf("pef = %08x, %08x\n", pef, propSize);
        inst = IOPEFNDRV::instantiate(regEntry, pef, propSize, handler, self);
        if (inst)
            regEntry->setProperty("AAPL,ndrvInst", inst);
    }
    else
        inst = 0;

    return (inst);
}

const char * IOPEFNDRV::driverName( void )
{
    return (fName);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#else  /* __ppc__ */

IONDRV * IOPEFNDRV::fromRegistryEntry( IORegistryEntry * regEntry,
				       OSData * newData,
                                       IONDRVUndefinedSymbolHandler handler,
                                       void * self )
{
    return (0);
}

#endif /* !__ppc__ */
