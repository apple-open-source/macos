/*
File:	    ApplePCCard16ATA.cpp

Contains:   The instantiation of the ApplePCCard16ATA class that acts as
            a translator between the ApplePCCardATA nub and the IOPCCard16Device
            nub that is published when an ATA PCCard is inserted.

Version:    1.1

Copyright:  © 2001-2003 by Apple Inc., all rights reserved.

File Ownership:

    DRI:	Mike Lum
    
    Other Contact: Larry Barras

Writers:    (Lum)    Mike Lum

Change History (most recent first):

    <3>     10/18/01	Lum	#ifdef-ed the custom enabler code. We don't really
                                need a custom enabler. Likewise with the probe method.
    <2>     10/17/01	Lum	Removed interruptSource, workLoop and other cruft.
    <1>     10/16/01    Lum	First Checked In
*/

#include "ApplePCCard16ATA.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Enabler Methods */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if _CustomEnabler
#undef  super
#define super IOPCCard16Enabler

OSDefineMetaClassAndStructors(ApplePCCard16ATAEnabler, IOPCCard16Enabler);

ApplePCCard16ATAEnabler *
ApplePCCard16ATAEnabler::withDevice(IOPCCard16Device *provider)
{    
    DLOG("ApplePCCard16ATAEnabler::withDevice entered\n");

    ApplePCCard16ATAEnabler *me = new ApplePCCard16ATAEnabler;

    if (me && !me->init(provider)) {
        me->release();
        return 0;
    }

    DLOG("ApplePCCard16ATAEnabler::withDevice exiting\n");

    return me;
}

bool
ApplePCCard16ATAEnabler::sortConfigurations(void)
{
    // a "custom" enabler could look for a specific configuration
    // and put it at the top of list if needed.
    
    DLOG("ApplePCCard16ATAEnabler::sortConfigurations entered\n");

    return super::sortConfigurations();
}
// _CustomEnabler
#endif 


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Driver Methods */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CardServices	nub->cardServices

#undef  super
#define super IOService

OSDefineMetaClassAndStructors(ApplePCCard16ATA, IOService);

bool
ApplePCCard16ATA::start(IOService * provider)
{        
    DLOG("ApplePCCard16ATA::start(provider=%p, this=%p) starting\n", provider, this);
    
    // Provider must be a 16-bit PCCard.
    nub = OSDynamicCast(IOPCCard16Device, provider);
    if (!nub) {
	DLOG("%s: provider is not of class IOPCCard16Device?\n", getName());
	return false;
    }

    // Show that we should be compatible with a PCCard ATA driver. The matching functions want
    // the compatible property to be of type 'data', not 'string', thus the indirection here.
    char *compatibleData = kApplePCCard16ATACompatibleValue;
    
    this->IORegistryEntry::setProperty(kApplePCCard16ATACompatibleKey, (void *)compatibleData, sizeof(kApplePCCard16ATACompatibleValue));

#if _CustomEnabler
    // this would be a good place to override the default enabler (if needed)
    DLOG("ApplePCCard16ATA: Creating custom enabler\n");
    ApplePCCard16ATAEnabler *customEnabler = ApplePCCard16ATAEnabler::withDevice(nub);
    if (!customEnabler) DLOG("%s: ApplePCCard16ATAEnabler::withDevice(nub) failed\n", getName());
    
    DLOG("ApplePCCard16ATA: Installing custom enabler\n");
    bool success = nub->installEnabler(customEnabler);
    if (!success) DLOG("%s: nub->installEnabler(customEnabler) failed\n", getName());
    
    customEnabler->release();	//Releasing the enabler prior to Puma SU1 causes a kernel panic...
#endif

    // configure card
    if (!nub->configure()) {
	stop(provider);
	return false;
    }

    // Ok, we're ready to go. Call registerService and exit.
    this->registerService();
    
    DLOG("ApplePCCard16ATA::start(provider=%p, this=%p) ending\n", provider, this);

    return true;
}


void
ApplePCCard16ATA::stop(IOService * provider)
{
    DLOG("ApplePCCard16ATA::stop(provider=%p, this=%p) starting\n", provider, this);
    
    if (nub != provider) {
	DLOG("%s: nub != provider\n", getName());
	return;
    }

    // release this device's enabler
    nub->unconfigure();

    super::stop(provider);
}


IOReturn
ApplePCCard16ATA::message(UInt32 type, IOService * provider, void * argument)
{
    UInt32 cs_arg = (UInt32) argument;

    switch( type )
    {
	
        case kIOMessageServiceIsTerminated:
            terminate();
            provider->close(this);
            break;
            
        case kIOPCCardCSEventMessage:
            DLOG("ApplePCCard16ATA::message, nub=%p, CS event received type=0x%x.\n", nub, (unsigned int)argument);
            // Card Services Events are defined in IOKit/pccard/cs.h
            switch( cs_arg )
            {
            	
            	case CS_EVENT_EJECTION_REQUEST:
            		return kIOReturnBusy;
            		break;
            	
                case CS_EVENT_CARD_REMOVAL:
                    if( messageClient( kATARemovedEvent, getClient(), (void*) OSString::withCString
                        ( kATAPCCardSocketString ), 0) != kIOReturnSuccess )
                    {		
                        DLOG("ApplePCCard16ATA did not handle device ejection because there are no children.\n");
                    }
                    break;
                            
                default:
                    break;
            }
            break;
            
        default:
            return super::message( type, provider, argument);
            break;
    }
	
    return kIOReturnSuccess;
}


IOMemoryMap * ApplePCCard16ATA::mapDeviceMemoryWithIndex (unsigned int index,
                                    IOOptionBits options = 0)
{
    return nub->mapDeviceMemoryWithIndex(index, options);
}