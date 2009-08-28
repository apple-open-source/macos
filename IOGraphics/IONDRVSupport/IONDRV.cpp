/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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


#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSContainers.h>

extern "C"
{
#include <pexpert/pexpert.h>
#include <mach/kmod.h>
};

#include "IONDRV.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndAbstractStructors(IONDRV, OSObject)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if __ppc__

enum
{ 
    kDate2001March1	= 0xb6c49300,
    kIOPEFMinROMDate	= kDate2001March1 
};

class IOPEFContainer : public OSData
{
    OSDeclareDefaultStructors(IOPEFContainer)

    OSData * fContainer;
    OSData * fDescription;

public:
    static IOPEFContainer * withData(OSData * data, OSData * description);
    virtual void free( void );
    virtual bool serialize(OSSerialize *s) const;
};

#include "IOPEFLoader.h"

#define LOG		if(1) kprintf

static IOLock  * gIOPEFLock;
static OSArray * gIOPEFContainers;

#define super IONDRV
OSDefineMetaClassAndStructorsWithInit(IOPEFNDRV, IONDRV, IOPEFNDRV::initialize());

#pragma options align=mac68k

struct CntrlParam {
    void *                          qLink;
    short                           qType;
    short                           ioTrap;
    void *                          ioCmdAddr;
    void *                          ioCompletion;
    short                           ioResult;
    char *                          ioNamePtr;
    short                           ioVRefNum;
    short                           ioCRefNum;
    short                           csCode;
    void *                          csParams;
    short                           csParam[9];
};
typedef struct CntrlParam CntrlParam, *CntrlParamPtr;
#pragma options align=reset

typedef SInt16	DriverRefNum;

enum {
    kOpenCommand                = 0,
    kCloseCommand               = 1,
    kReadCommand                = 2,
    kWriteCommand               = 3,
    kControlCommand             = 4,
    kStatusCommand              = 5,
    kKillIOCommand              = 6,
    kInitializeCommand          = 7,                            /* init driver and device*/
    kFinalizeCommand            = 8,                            /* shutdown driver and device*/
    kReplaceCommand             = 9,                            /* replace an old driver*/
    kSupersededCommand          = 10                            /* prepare to be replaced by a new driver*/
};
enum {
    kSynchronousIOCommandKind   = 0x00000001,
    kAsynchronousIOCommandKind  = 0x00000002,
    kImmediateIOCommandKind     = 0x00000004
};
struct DriverInitInfo {
	DriverRefNum	refNum;
	RegEntryID		deviceEntry;
};
typedef struct DriverInitInfo DriverInitInfo; 

typedef DriverInitInfo *			DriverInitInfoPtr;
typedef DriverInitInfo				DriverReplaceInfo;
typedef DriverInitInfo *			DriverReplaceInfoPtr;

struct DriverFinalInfo {
	DriverRefNum	refNum;
	RegEntryID	deviceEntry;
};
typedef struct DriverFinalInfo DriverFinalInfo; 
typedef DriverFinalInfo *			DriverFinalInfoPtr;
typedef DriverFinalInfo				DriverSupersededInfo;
typedef DriverFinalInfo *			DriverSupersededInfoPtr;

// Contents are command specific
union ParamBlockRec;
typedef union ParamBlockRec ParamBlockRec;
typedef ParamBlockRec *ParmBlkPtr;

union IOCommandContents {
	ParmBlkPtr				pb;
	DriverInitInfoPtr		initialInfo;
	DriverFinalInfoPtr		finalInfo;
	DriverReplaceInfoPtr	replaceInfo;
	DriverSupersededInfoPtr	supersededInfo;
};
typedef union IOCommandContents IOCommandContents; 

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOPEFNDRV::initialize( void )
{
    if (!gIOPEFLock)
	gIOPEFLock = IOLockAlloc();
    if (!gIOPEFContainers)
	gIOPEFContainers = OSArray::withCapacity(2);
}

IOPEFNDRV * IOPEFNDRV::instantiate( IORegistryEntry * regEntry,
				     IOLogicalAddress container,
				     IOByteCount containerSize,
				     bool checkDate,
				     IONDRVUndefinedSymbolHandler undefHandler,
				     void * self )
{
    OSStatus	err = 1;
    IOPEFNDRV *	inst;
    char * 	name;
    IOByteCount	plen;
    UInt32	createDate;
#if CREATE_PEF_KMOD
    char	kmodName[KMOD_MAX_NAME * 2];
    char	kmodVers[KMOD_MAX_NAME];
#endif

    inst = new IOPEFNDRV;

    if (inst)
    {
        do
        {
            if (false == inst->init())
                continue;

            err = PCodeOpen( (void *)container, containerSize, &inst->fPEFInst, &createDate );
            if (err)
                continue;

            err = PCodeInstantiate( inst->fPEFInst, undefHandler, self );
            if (err)
                continue;

	    if (checkDate && createDate && (createDate < kIOPEFMinROMDate))
	    {
		int	debugFlags;
	
		IOLog("ROM ndrv for %s is too old (0x%08lx)\n", regEntry->getName(), createDate);
		if (!PE_parse_boot_argn("romndrv", &debugFlags, sizeof(debugFlags)) || !debugFlags)
		{
		    err = kIOReturnIsoTooOld;
		    continue;
		}
	    }

            err = inst->getSymbol("DoDriverIO", (IOLogicalAddress *) &inst->fDoDriverIO);
            if (err)
                continue;
            err = inst->getSymbol("TheDriverDescription",
				    (IOLogicalAddress *) &inst->fDriverDesc);
            if (err)
                continue;
	    name = (char *) inst->fDriverDesc->driverOSRuntimeInfo.driverName;
	    plen = name[ 0 ];
	    if (plen >= sizeof(inst->fDriverDesc->driverOSRuntimeInfo.driverName))
		plen = sizeof(inst->fDriverDesc->driverOSRuntimeInfo.driverName) - 1;
	    strncpy( inst->fName, name + 1, plen);
#if 1
	    inst->fName[plen] = 0;
#else
	    sprintf( inst->fName + plen, "-%08lx", *((UInt32 *) &inst->fDriverDesc->driverType.version));
#endif

#if CREATE_PEF_KMOD
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

            if (KERN_SUCCESS != kmod_create_fake_with_address(
                    kmodName, kmodVers,
                    round_page((vm_address_t) container), 
                    trunc_page((vm_size_t) container + containerSize) 
                        - round_page((vm_address_t) container),
                    &inst->fKModID))
                inst->fKModID = 0;
#endif /* CREATE_PEF_KMOD */

        }
        while (false);
    }
    if (inst && err)
    {
        inst->release();
        inst = 0;
    }

    return (inst);
}

void IOPEFNDRV::free( void )
{
#if CREATE_PEF_KMOD
    if (fKModID)
        kmod_destroy_fake(fKModID);
#endif

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

OSDefineMetaClassAndStructors(IOPEFContainer, OSData);

IOPEFContainer * IOPEFContainer::withData(OSData * data, OSData * description)
{
    IOPEFContainer * inst;

    if (!data || !description)
	return (0);

    inst = new IOPEFContainer;
    if (inst && !inst->initWithBytesNoCopy((void *) data->getBytesNoCopy(),
					    data->getLength()))
    {
	inst->release();
	inst = 0;
    }
    if (inst)
    {
	data->retain();
	inst->fContainer = data;
	description->retain();
	inst->fDescription = description;
    }

    return (inst);
}

void IOPEFContainer::free( void )
{
    if (fContainer)
	fContainer->release();
    if (fDescription)
	fDescription->release();
    OSData::free();
}

bool IOPEFContainer::serialize(OSSerialize *s) const
{
    return (fDescription->serialize(s));
}

IOPEFNDRV * IOPEFNDRV::fromRegistryEntry( IORegistryEntry * regEntry,
				       OSData * newData,
                                       IONDRVUndefinedSymbolHandler handler,
                                       void * self )
{
    IOLogicalAddress	pef = 0;
    IOByteCount		propSize = 0;
    OSData *		prop;
    IOPEFNDRV *		inst;
    unsigned int 	i;
    bool		checkDate;

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
	checkDate = false;
    }
    else
    {
	inst = (IOPEFNDRV *) regEntry->copyProperty("AAPL,ndrvInst");
	if (inst)
	    return (inst);
	prop = (OSData *) regEntry->getProperty("driver,AAPL,MacOS,PowerPC");
	checkDate = true;
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
	IOLockUnlock(gIOPEFLock);
        pef = (IOLogicalAddress) prop->getBytesNoCopy();
        propSize = prop->getLength();
    }

    if (pef)
    {
        inst = IOPEFNDRV::instantiate(regEntry, pef, propSize, checkDate, handler, self);
        if (inst)
	{
	    if (!newData)
	    {
		OSData * description;
		IOPEFContainer * pefData;

		description = OSData::withBytes(inst->fDriverDesc, sizeof(DriverDescription));
		pefData = IOPEFContainer::withData(prop, description);
		if (pefData)
		{
		    IOLockLock(gIOPEFLock);
		    gIOPEFContainers->setObject(pefData);
		    pefData->release();
		    prop = pefData;
		    IOLockUnlock(gIOPEFLock);

		}
		if (description)
		    description->release();
	    }
	    regEntry->setProperty("driver,AAPL,MacOS,PowerPC", prop);
            regEntry->setProperty("AAPL,ndrvInst", inst);
	}
	else if (checkDate)
	    regEntry->removeProperty("driver,AAPL,MacOS,PowerPC");
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

#endif /* __ppc__ */
