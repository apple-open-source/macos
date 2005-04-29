/* ====================================================================
© Copyright 2001 Apple Computer, Inc. All rights reserved.

IMPORTANT:  This Apple software is supplied to you by Apple Computer,
Inc. ("Apple") in consideration of your agreement to the following terms,
and your use, installation, modification or redistribution of this Apple
software constitutes acceptance of these terms.  If you do not agree with
these terms, please do not use, install, modify or redistribute this
Apple software.

In consideration of your agreement to abide by the following terms, and
subject to these terms, Apple grants you a personal, non-exclusive
license, under Apple's copyrights in this original Apple software (the
"Apple Software"), to use, reproduce, modify and redistribute the Apple
Software, with or without modifications, in source and/or binary forms;
provided that if you redistribute the Apple Software in its entirety and
without modifications, you must retain this notice and the following text
and disclaimers in all such redistributions of the Apple Software.
Neither the name, trademarks, service marks or logos of Apple Computer,
Inc. may be used to endorse or promote products derived from the Apple
Software without specific prior written permission from Apple.  Except as
expressly stated in this notice, no other rights or licenses, express or
implied, are granted by Apple herein, including but not limited to any
patent rights that may be infringed by your derivative works or by other
works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES
NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE
IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION
ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
============================================================================ */

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pccard/IOPCCard.h>

// most of the time drivers will not need a custom enabler to get their
// card configured properly, below is quick example of how to set up 
// a enabler and then in the start method, how to register it with the 
// system.  if your driver does not register a enabler, the system will use
// the built in default enabler.  hence, most of time the code below is
// now required to have a functioning driver.

#define NEEDS_A_ENABLER

#ifdef NEEDS_A_ENABLER

class ApplePCCardSampleEnabler : public IOPCCard16Enabler
{
    OSDeclareDefaultStructors(ApplePCCardSampleEnabler);

public:
    static ApplePCCardSampleEnabler * withDevice(IOPCCard16Device *provider);
    virtual bool sortConfigurations(void);
};

#undef  super
#define super IOPCCard16Enabler

OSDefineMetaClassAndStructors(ApplePCCardSampleEnabler, IOPCCard16Enabler);

ApplePCCardSampleEnabler *
ApplePCCardSampleEnabler::withDevice(IOPCCard16Device *provider)
{
    ApplePCCardSampleEnabler *me = new ApplePCCardSampleEnabler;

    if (me && !me->init(provider)) {
        me->free();
        return 0;
    }

    return me;
}

bool
ApplePCCardSampleEnabler::sortConfigurations(void)
{
    // the most common use of a "custom" enabler could look for a
    // specific configuration and put it at the top of list if needed.
    // this would most likely be done in a driver that matches against
    // a class of cards (like serial, ata, ...). The default enabler
    // sorts the available configurations by the number of windows
    // that are presented, preferring memory windows.

    // if a driver is matching against a particular card, it can
    // just ask for a specific configuration when calling the nub's
    // configure method.  see the start method below.
    
    IOLog("ApplePCCardSampleEnabler::sortConfigurations entered\n");

    IOSleep(500);
    for (unsigned i=0; i < tableEntryCount; i++) {
	cistpl_cftable_entry_t *cfg = configTable[i];

	IOLog("sortConfig index=0x%x, vcc %d vpp %d vpp1 %d", cfg->index, 
	      (cfg->vcc.present  & (1<<CISTPL_POWER_VNOM)) ? (int)cfg->vcc.param[CISTPL_POWER_VNOM] : 0,
	      (cfg->vpp1.present & (1<<CISTPL_POWER_VNOM)) ? (int)cfg->vpp1.param[CISTPL_POWER_VNOM] : 0,
	      (cfg->vpp2.present & (1<<CISTPL_POWER_VNOM)) ? (int)cfg->vpp2.param[CISTPL_POWER_VNOM] : 0);
	if (cfg->io.nwin) {
	    IOLog(" io (desc=0x%x)", cfg->io.flags);
	    for (unsigned j=0; j < cfg->io.nwin; j++) {
		IOLog(", 0x%x-0x%x", cfg->io.win[j].base, cfg->io.win[j].base + cfg->io.win[j].len - 1);
	    }
	}
	if (cfg->mem.nwin) {
	    IOLog(" mem sizes");
	    for (unsigned j=0; j < cfg->mem.nwin; j++) {
		IOLog(" 0x%x", cfg->mem.win[j].len);
	    }
	}
	IOLog("\n");
    }
    IOSleep(500);
    
    return super::sortConfigurations();
}

#endif NEEDS_A_ENABLER

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class ApplePCCardSample : public IOService
{
    OSDeclareDefaultStructors(ApplePCCardSample)

    IOPCCard16Device 			* nub;

    IOInterruptEventSource              * interruptSource;
    IOWorkLoop                          * workLoop;

    unsigned				windowCount;
    IOMemoryMap 			* windowMap[10];
    
    bool				cardPresent;

public:
    virtual IOService * probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService * provider);
    virtual void stop(IOService * provider);

    virtual IOReturn setPowerState(unsigned long powerState, IOService * whatDevice);
    virtual IOReturn message(UInt32 type, IOService * provider, void * argument = 0);

    static void interruptOccurred(class ApplePCCardSample * sample, IOInterruptEventSource * src, int i);

    void dumpWindows();
    void dumpConfigRegisters();
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CardServices	nub->cardServices

#undef  super
#define super IOService

OSDefineMetaClassAndStructors(ApplePCCardSample, IOService);

IOService*
ApplePCCardSample::probe(IOService * provider, SInt32 * score)
{
    IOLog("ApplePCCardSample::probe(provider=%p, score=0x%x) starting\n", provider, (int)*score);
    
    nub = OSDynamicCast(IOPCCard16Device, provider);
    if (!nub) return NULL;

    cardPresent = true;

    if (!super::probe(provider, score)) return NULL;

    // most of the time you will not need to override probe to
    // determine if this is the correct driver.  it is much better to
    // use passive matching (in the XML file) then to actually load
    // the driver into the kernel and call this probe method.

    // however, this is a good place to to check if this is actually
    // our card. if not we can reject it right here by returning NULL.
    // we might want to do this if this driver is matching against a
    // class of cards.  another check might be to see if the hardware
    // above the card can support the card, for example zoom video.

    // the score passed in comes from this driver's XML file, normally
    // there is no need to mess with it

    // the recommended match score for a pc card driver that matches
    // against a class of cards is 1000.  the recommended match score
    // for a driver that matches against a specific card is 10000.
    
    // to see if this is the right driver you can check nubs properties
    // and or read extra cis tuples, ... here.  remember other drivers
    // may also be probing the same card, you should not assume that
    // any state is preserved between here and start.

    // let's try scanning the CIS (for the fun of it, you don't need this in a real driver)
    cisinfo_t cisinfo;
    int ret = CardServices(ValidateCIS, nub->getCardServicesHandle(), &cisinfo);
    if (ret != CS_SUCCESS) {
	IOLog("ApplePCCardSample: ValidateCIS failed %d.\n", ret);
	stop(provider);
	return false;
    }
    IOLog("ApplePCCardSample: ValidateCIS Chains=%d.\n", cisinfo.Chains);

    // if (not our card) return NULL;

    return this;
}


bool
ApplePCCardSample::start(IOService * provider)
{
    IOLog("ApplePCCardSample::start(provider=%p, this=%p) starting\n", provider, this);
    
    nub = OSDynamicCast(IOPCCard16Device, provider);
    if (!nub) {
	IOLog("%s: provider is not of class IOPCCard16Device?\n", getName());
	return false;
    }

//#define CHANGE_VERSION_ONE 1
#ifdef  CHANGE_VERSION_ONE
    OSArray *vers1 = OSArray::withCapacity(2);
    if (vers1) {
	const OSSymbol *str = OSSymbol::withCString("test vendor");
	if (str) {
	    vers1->setObject((OSSymbol *)str);
	    str->release();
	}
	str = OSSymbol::withCString("test card type");
	if (str) {
	    vers1->setObject((OSSymbol *)str);
	    str->release();
	}
	provider->setProperty("VersionOneInfo", vers1);
	vers1->release();
    }
#endif

    cardPresent = true;

    static const IOPMPowerState myPowerStates[ kIOPCCard16DevicePowerStateCount ] = 
    {
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    // initialize our PM superclass variables
    PMinit();
    // register as the controlling driver
    registerPowerDriver(this, (IOPMPowerState *)myPowerStates, kIOPCCard16DevicePowerStateCount);
    // add ourselves into the PM tree
    provider->joinPMtree( this);
    // set current pm state
    changePowerStateTo(kIOPCCard16DeviceOnState);

    // get the default pccard family workloop
    workLoop = getWorkLoop();
    if (!workLoop) {
        IOLog("%s: could not get the workLoop.\n", getName());
        return false;
    }
    
    // create a interrupt event source
    interruptSource = IOInterruptEventSource::interruptEventSource(this,
								   (IOInterruptEventAction)&ApplePCCardSample::interruptOccurred,
								   provider, 
								   0);
    if (!interruptSource) {
        IOLog("%s: Failed to create interrupt event source.\n", getName());
        return false;
    }
    
    // register Interrupt Event Source with the workloop.
    if (workLoop->addEventSource(interruptSource) != kIOReturnSuccess) {
        IOLog("%s: Failed to add interrupt event to workLoop.\n", getName());
	interruptSource->release();
	interruptSource = NULL;
	workLoop->release();
	workLoop = NULL;
        return false;
    }

    // enable the interrupt delivery.
    workLoop->enableAllInterrupts();
    
#ifdef NEEDS_A_ENABLER

    // this is a good place to override the default enabler (if needed)
    // the nub handles releasing the enabler so we can forget about it.

    ApplePCCardSampleEnabler *customEnabler = ApplePCCardSampleEnabler::withDevice(nub);
    if (!customEnabler) IOLog("%s: ApplePCCardSampleEnabler::withDevice(nub) failed\n", getName());
    
    bool success = nub->installEnabler(customEnabler);
    if (!success) IOLog("%s: nub->installEnabler(customEnabler) failed\n", getName());
    customEnabler->release();

#endif
    
    // configure card, if the card's CIS contains multiple configurations, the enabler will
    // select the card with the least number of resources, if this turns out to not work
    // for your driver, you may also specify the index of the configuration that you want
    // by calling configure(index) or by overriding the default enabler.
    
    if (!nub->configure()) {
	stop(provider);
	return false;
    }
    
    config_info_t config;
    if (!nub->getConfigurationInfo(&config)) IOLog("%s: getConfigurationInfo failed\n", getName());    
    IOLog("%s: the client is %svalid, ConfigBase=0x%x, BasePort1=0x%x(0x%x), BasePort2=0x%x(0x%x)\n",
	    getName(), (config.Attributes & CONF_VALID_CLIENT) ? "" : "not ", config.ConfigBase, 
	    config.BasePort1, config.NumPorts1, config.BasePort2, config.NumPorts2);
    
    // find out how many windows we have configured
    windowCount = nub->getWindowCount();

    // map in the windows
    for (unsigned i=0; i < windowCount; i++) {
    
	UInt32 attributes;
	if (!nub->getWindowAttributes(i, &attributes)) IOLog("%s: getWindowAttributes failed\n", getName());

	IOLog("%s: mapping window index %d, size = 0x%x, attributes = 0x%x\n", getName(), i, (int)nub->getWindowSize(i), (int)attributes);
	windowMap[i] = nub->mapDeviceMemoryWithIndex(i);
	if (!windowMap[i]) {
	    IOLog("%s: Failed to map device memory index %d.\n", getName(), i);
	    stop(provider);
	    return false;
	}
    }

    dumpWindows();
    dumpConfigRegisters();

    IOLog("ApplePCCardSample::start(provider=%p, this=%p) ending\n", provider, this);

    return true;
}

void
ApplePCCardSample::stop(IOService * provider)
{
    IOLog("ApplePCCardSample::stop(provider=%p, this=%p) stopping\n", provider, this);
    
    if (nub != provider) {
	IOLog("%s: nub != provider\n", getName());
	return;
    }

    // unmap the windows
    for (unsigned i=0; i < windowCount; i++) {

	IOLog("%s: unmapping window index %d, size = 0x%x\n", getName(), i, (int)nub->getWindowSize(i));

	if (windowMap[i]) windowMap[i]->release();
    }


    // release this device's enabler
    nub->unconfigure();

    // take ourselves out of PM tree
    PMstop();

    if (workLoop && interruptSource) {
	workLoop->removeEventSource(interruptSource);
	interruptSource->release();
	interruptSource = NULL;
	workLoop = NULL;
    }

    super::stop(provider);
}

IOReturn
ApplePCCardSample::setPowerState(unsigned long powerState, IOService * whatDevice)
{

    switch (powerState) {

    case kIOPCCard16DeviceOnState:
    case kIOPCCard16DeviceDozeState:

	// this is where you can either restore your cards state from 
	// what was saved below, or reinitialize your state from scratch
	
	// unless you can do something special with the doze state
	// you can just treat it the same as the on state.

	IOLog("ApplePCCardSample::setPowerState setting power state to on\n");

	// reenable the interrupt delivery.
	if (workLoop) workLoop->enableAllInterrupts();

	// are we still alive?
	dumpWindows();

	break;

    case kIOPCCard16DeviceOffState:

	// the card is going to be powered off after this method returns
	// this is a good place to save your device's state.

	// any windows that were created by calling
	// mapDeviceMemoryWithIndex() will be unmapped by the PC Card
	// Family spanning the time between the off and on power
	// states.  If you have interrupts, timers (or whatever) that
	// could go off during this time, you should either stop them
	// here or keep track of your device's state with this
	// object's instance variables to avoid accessing any of these
	// windows.

	// this is a good idea even if you are not expecting any
	// interrupts, some cards generate spurious interrupts
	if (workLoop) workLoop->disableAllInterrupts();

	IOLog("ApplePCCardSample::setPowerState setting power state to off\n");
	break;
	
    default:

	IOLog("ApplePCCardSample::setPowerState unknown state=%d?\n", (int)powerState);
	break;
    }

    return IOPMAckImplied;
}

IOReturn
ApplePCCardSample::message(UInt32 type, IOService * provider, void * argument)
{
    switch(type) {
	 
    case kIOPCCardCSEventMessage: {

	// by default, the events types below are registered for your
	// driver.  you can change these if you like.  the two most
	// important ones are "card removal" and "ejection request".
	// the events types are described in more detail in the
	// documentation

	// note: unlike the linux pc card drivers, the events for
	// CS_EVENT_PM_SUSPEND and CS_EVENT_PM_RESUME are not used
	// here.  you should instead rely on the normal iokit power
	// management methods for notification.

	UInt32 cs_event = (UInt32) argument;

	switch (cs_event) {

	case CS_EVENT_CARD_INSERTION:
	
	    IOLog("ApplePCCardSample::message, nub=%p, card services event CS_EVENT_CARD_INSERTION.\n", nub);

	    // card services generates a fake insertion when a driver
	    // registers with it

	    break;

	case CS_EVENT_CARD_REMOVAL:

	    IOLog("ApplePCCardSample::message, nub=%p, card services event CS_EVENT_CARD_REMOVAL.\n", nub);

	    // this event is telling you that the card has been
	    // removed, this is a good place to set a flag telling the
	    // driver that the hardware is no longer present
	    
	    cardPresent = false;

	    // note: if the card has been removed your driver will
	    // also be terminated very shortly after receiving this
	    // message.  regardless, if you try to do something here
	    // or not there may still be a race between you accessing
	    // the hardware and this event.  the pc card bridge chips
	    // on all apple hardware will allow accesses through the
	    // configured hardware windows even if the card has been
	    // removed so you will not crash the system.  obviously
	    // you won't be talking to the card though.
	    break;

	case CS_EVENT_EJECTION_REQUEST:

	    IOLog("ApplePCCardSample::message, nub=%p, card services event CS_EVENT_EJECTION_REQUEST.\n", nub);

	    // the user or system is asking if it is safe to shutdown
	    // and eject the card, you can say no here by returning
	    // kIOReturnBusy.
	    break;

	case CS_EVENT_RESET_REQUEST:

	    IOLog("ApplePCCardSample::message, nub=%p, card services event CS_EVENT_RESET_REQUEST.\n", nub);

	    // card services is asking if is is ok to reset the card.
	    // this usually only interesting if there are multiple
	    // clients for this card
	    break;

	case CS_EVENT_RESET_PHYSICAL:

	    IOLog("ApplePCCardSample::message, nub=%p, card services event CS_EVENT_RESET_PHYSICAL.\n", nub);

	    // the card is about to be reset
	    break;

	case CS_EVENT_CARD_RESET:

	    IOLog("ApplePCCardSample::message, nub=%p, card services event CS_EVENT_CARD_RESET.\n", nub);

	    // the card was reset
	    break;

	default:
	
	    IOLog("ApplePCCardSample::message, nub=%p, card services event type=0x%x.\n", nub, (int)cs_event);
	    break;
	}
    }
    break;

    default:

	return super::message(type, provider, argument);
	break;
    }

    return kIOReturnSuccess;
}

void
ApplePCCardSample::interruptOccurred(class ApplePCCardSample * sample, IOInterruptEventSource * src, int i)
{
    IOLog("ApplePCCardSample::interruptOccurred, nub=%p, src=%p, i=0x%x.\n", sample->nub, src, i);
    if (!sample->cardPresent) {
	IOLog("%s::interruptOccurred, ignoring interrupt, the card is not present\n", sample->getName());
	return;
    }

}

void
ApplePCCardSample::dumpWindows()
{
    if (!cardPresent) {
	IOLog("%s::dumpWindows, aborting, the card is not present\n", getName());
	return;
    }
    
    for (unsigned i=0; i < windowCount; i++) {
    
	if (nub->getWindowType(i) == IOPCCARD16_MEMORY_WINDOW) {
	    UInt32 offset;
	    if (!nub->getWindowOffset(i, &offset)) IOLog("%s: getWindowOffset failed\n", getName());
	    IOLog("%s: window %d physical 0x%x virtual 0x%x offset 0x%x length 0x%x (memory)\n", 
		  getName(), i, (int)windowMap[i]->getPhysicalAddress(), windowMap[i]->getVirtualAddress(), (int)offset, (int)windowMap[i]->getLength());

	    IOLog("%x %x %x %x %x %x %x %x\n", 
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 0),  (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 4),
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 8),  (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 12),
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 16), (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 20),
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 24), (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 28));
		
#if 1
	    // try moving window offset to 0x1000
	    if (!nub->setWindowOffset(i, 0x1000)) IOLog("%s: setWindowOffset failed\n", getName());
	    if (!nub->getWindowOffset(i, &offset)) IOLog("%s: getWindowOffset failed (case 2)\n", getName());
	    IOLog("%s: window %d physical 0x%x virtual 0x%x offset 0x%x length 0x%x (memory)\n", 
		  getName(), i, (int)windowMap[i]->getPhysicalAddress(), windowMap[i]->getVirtualAddress(), (int)offset, (int)windowMap[i]->getLength());

	    IOLog("%x %x %x %x %x %x %x %x\n", 
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 0),  (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 4),
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 8),  (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 12),
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 16), (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 20),
		(int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 24), (int)OSReadSwapInt32((void *)windowMap[i]->getVirtualAddress(), 28));
#endif	    
	} else {
	    IOLog("%s: window %d physical 0x%x virtual 0x%x length 0x%x (io)\n", 
		  getName(), i, (int)windowMap[i]->getPhysicalAddress(), windowMap[i]->getVirtualAddress(), (int)windowMap[i]->getLength());

	    UInt32 attributes;
	    if (!nub->getWindowAttributes(i, &attributes)) {
		IOLog("%s: getWindowAttributes failed\n", getName());
		return;
	    }

	    if (attributes & IO_DATA_PATH_WIDTH == IO_DATA_PATH_WIDTH_16) {
		int l = windowMap[i]->getLength(); l = l > 0x10 ? 0x10 : l;
		for (int j=0; j < l; j++,j++) {
		    IOLog("%x ", nub->ioRead16(j, windowMap[i]));
		}
	    } else {
		int l = windowMap[i]->getLength(); l = l > 0x10 ? 0x10 : l;
		for (int j=0; j < l; j++) {
		    IOLog("%x ", nub->ioRead8(j, windowMap[i]));
		}
	    }
	    IOLog("\n");
	}
    }
}


// attribute memory usually only makes use of even byte addresses
#define ATTRIBUTE_SIZE 0x1000

void
ApplePCCardSample::dumpConfigRegisters()
{
    if (!cardPresent) {
	IOLog("%s::dumpConfigRegisters, the card is not present\n", getName());
	return;
    }

    config_info_t config;
    if (!nub->getConfigurationInfo(&config)) {
	IOLog("%s:dumpConfigRegisters, getConfigurationInfo failed\n", getName());
	return;
    }

    OSData * temp = OSData::withCapacity(ATTRIBUTE_SIZE >> 1);
    
    conf_reg_t reg;
    reg.Function = 0;		// this should always be zero when used this way
    reg.Action = CS_READ;	// read (or CS_WRITE)
    reg.Offset = 0;		// offset into attribute memory
    reg.Value = 0;		// value return (or value to write)

    for (int i=0; i < (ATTRIBUTE_SIZE - (int)config.ConfigBase); i += 2) {
	     
	reg.Offset = i;
	int ret = CardServices(AccessConfigurationRegister, nub->getCardServicesHandle(), &reg);
	if (ret != CS_SUCCESS) {
	    IOLog("%s::dumpConfigRegisters failed %d.\n", getName(), ret);
	    break;
	}
	UInt8 byte = (UInt8)reg.Value;
	temp->appendBytes(&byte, 1);
    }

    // make us visible at user level so we can read these properties
    registerService();  

    // set properties for dump_cisreg to pickup
    setProperty("Configuration Registers Base Address", config.ConfigBase, 32);
    setProperty("Configuration Registers Present Mask", config.Present, 32);
    setProperty("Attribute Memory", temp);
    temp->release();
}
