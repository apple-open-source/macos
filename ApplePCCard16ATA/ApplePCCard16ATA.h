/*
File:	    ApplePCCard16ATA.h

Contains:   Header file containing #includes, constants, flags and class definitions
            for the ApplePCCard16ATA and ApplePCCard16ATACustomEnabler classes.

Version:    1.1

Copyright:  © 2001 by Apple Inc., all rights reserved.

File Ownership:

    DRI:	Mike Lum
    
    Other Contact: Larry Barras

Writers:    (Lum)    Mike Lum

Change History (most recent first):

    <3>     10/18/01	Lum	#ifdef-ed the custom enabler code. We don't really
                                need a custom enabler. Removed the probe method.
    <2>     10/17/01	Lum	Removed interruptSource, workLoop and other cruft.
    <1>     10/16/01    Lum	First Checked In
*/

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/pccard/IOPCCard.h>

#ifdef DLOG
#undef DLOG
#endif

// Debug Flags
#define _PCCard_DEBUG	0
#define _CustomEnabler	0

#ifdef  _PCCard_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define kApplePCCard16ATACompatibleKey		"compatible"
#define kApplePCCard16ATACompatibleValue	"pccard-ata"
#define kApplePCCard16ATAMaxWindows 		8

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


class ApplePCCard16ATA : public IOService
{
    OSDeclareDefaultStructors(ApplePCCard16ATA)

    IOPCCard16Device 			* nub;

public:
    virtual bool start(IOService * provider);
    virtual void stop(IOService * provider);
    virtual IOMemoryMap * mapDeviceMemoryWithIndex (unsigned int index,
                                    IOOptionBits options = 0);

    virtual IOReturn message(UInt32 type, IOService * provider, void * argument = 0);
};


#if _CustomEnabler
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class ApplePCCard16ATAEnabler : public IOPCCard16Enabler
{
    OSDeclareDefaultStructors(ApplePCCard16ATAEnabler);

public:
    static ApplePCCard16ATAEnabler * withDevice(IOPCCard16Device *provider);
    virtual bool sortConfigurations(void);
};
// _CustomEnabler
#endif
