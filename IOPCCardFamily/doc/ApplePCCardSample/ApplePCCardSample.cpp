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
license, under Apple’s copyrights in this original Apple software (the
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
    // a "custom" enabler could look for a specific configuration
    // and put it at the top of list if needed.
    
    IOLog("ApplePCCardSampleEnabler::sortConfigurations entered\n");

    return super::sortConfigurations();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class ApplePCCardSample : public IOService
{
    OSDeclareDefaultStructors(ApplePCCardSample)

    IOPCCard16Device 			* nub;

    IOInterruptEventSource              * interruptSource;
    IOWorkLoop                          * workLoop;

public:
    virtual IOService * probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService * provider);
    virtual void stop(IOService * provider);

    virtual IOReturn message(UInt32 type, IOService * provider, void * argument = 0);

    void interruptOccurred(IOInterruptEventSource * src, int i);
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

    if (!super::probe(provider, score)) return NULL;

    // the score passed in comes from this driver's XML file, normally
    // there is no need to mess with it, or to even subclass probe()
    // in the first place.  

    // however, this is a good place to to check if this is actually
    // our card. if not we can reject it right here by returning NULL.
    // we might want to do this if this driver is matching against a
    // class of cards.  another check might be to see if the hardware
    // above the card can support the card, for example zoom video.

    // the recommended match score for a driver that matches against a
    // class of cards is 1000.  the recommended match score for a
    // driver that matches against a specific card is 10000.
    
    // check nub's properties and/or read extra cis tuples, ...

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
    
    // get the default pccard workloop
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
    
    // try scanning the CIS (for the fun of it, you don't need this in a real driver)
    cisinfo_t cisinfo;
    int ret = CardServices(ValidateCIS, nub->getCardServicesHandle(), &cisinfo);
    if (ret != CS_SUCCESS) {
	IOLog("ApplePCCardSample: ValidateCIS failed %d.\n", ret);
	stop(provider);
	return false;
    }
    IOLog("ApplePCCardSample: ValidateCIS Chains=%d.\n", cisinfo.Chains);

    // this is a good place to override the default enabler (if needed)
    // the nub handles releasing the enabler so we can forget about it.
#if 1
    ApplePCCardSampleEnabler *customEnabler = ApplePCCardSampleEnabler::withDevice(nub);
    if (!customEnabler) IOLog("%s: ApplePCCardSampleEnabler::withDevice(nub) failed\n", getName());
    
    bool success = nub->installEnabler(customEnabler);
    if (!success) IOLog("%s: nub->installEnabler(customEnabler) failed\n", getName());
    customEnabler->release();
#endif	
    
    // configure card, if the card's CIS contains multiple configurations, the enabler will
    // select the card with the least number of resources, if this turns out to not work
    // for your driver, you may also specify the index of the configuration that you want
    // by calling configure(index).
    
    if (!nub->configure()) {
	stop(provider);
	return false;
    }
    
    config_info_t config;
    if (!nub->getConfigurationInfo(&config)) IOLog("%s: getConfigurationInfo failed\n", getName());    
    IOLog("%s: the client is %svalid, ConfigBase=0x%x, BasePort1=0x%x(0x%x), BasePort2=0x%x(0x%x)\n",
	    getName(), (config.Attributes & CONF_VALID_CLIENT) ? "" : "not ", config.ConfigBase, 
	    config.BasePort1, config.NumPorts1, config.BasePort2, config.NumPorts2);
    
    unsigned windowCount = nub->getWindowCount();
        
    for (unsigned i=0; i < windowCount; i++) {
    
	UInt32 attributes;
	if (!nub->getWindowAttributes(i, &attributes)) IOLog("%s: getWindowAttributes failed\n", getName());

	IOLog("%s: mapping window index %d, size = 0x%x, attributes = 0x%x\n", getName(), i, (int)nub->getWindowSize(i), (int)attributes);
	IOMemoryMap * map = nub->mapDeviceMemoryWithIndex(i);
	if (!map) {
	    IOLog("%s: Failed to map device memory index %d.\n", getName(), i);
	    stop(provider);
	    return false;
	}

	if (nub->getWindowType(i) == IOPCCARD16_MEMORY_WINDOW) {
	    UInt32 offset;
	    if (!nub->getWindowOffset(i, &offset)) IOLog("%s: getWindowOffset failed\n", getName());
	    IOLog("%s: window %d physical 0x%x virtual 0x%x offset 0x%x length 0x%x (memory)\n", 
		  getName(), i, (int)map->getPhysicalAddress(), map->getVirtualAddress(), (int)offset, (int)map->getLength());

	    IOLog("%x %x %x %x %x %x %x %x\n", 
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 0),  (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 4),
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 8),  (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 12),
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 16), (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 20),
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 24), (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 28));
		
#if 1
	    // move window offset to 0x1000
	    if (!nub->setWindowOffset(i, 0x1000)) IOLog("%s: setWindowOffset failed\n", getName());
	    if (!nub->getWindowOffset(i, &offset)) IOLog("%s: getWindowOffset failed (case 2)\n", getName());
	    IOLog("%s: window %d physical 0x%x virtual 0x%x offset 0x%x length 0x%x (memory)\n", 
		  getName(), i, (int)map->getPhysicalAddress(), map->getVirtualAddress(), (int)offset, (int)map->getLength());

	    IOLog("%x %x %x %x %x %x %x %x\n", 
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 0),  (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 4),
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 8),  (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 12),
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 16), (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 20),
		(int)OSReadSwapInt32((void *)map->getVirtualAddress(), 24), (int)OSReadSwapInt32((void *)map->getVirtualAddress(), 28));
#endif	    
	} else {
	    IOLog("%s: window %d physical 0x%x virtual 0x%x length 0x%x (io)\n", 
		  getName(), i, (int)map->getPhysicalAddress(), map->getVirtualAddress(), (int)map->getLength());

	    // I don't think the data path width even matters on apple hardware, but ...
	    if (attributes & IO_DATA_PATH_WIDTH == IO_DATA_PATH_WIDTH_16) {
		int l = map->getLength(); l = l > 0x10 ? 0x10 : l;
		for (int j=0; j < l; j++,j++) {
		    IOLog("%x ", nub->ioRead16(j, map));
		}
	    } else {
		int l = map->getLength(); l = l > 0x10 ? 0x10 : l;
		for (int j=0; j < l; j++) {
		    IOLog("%x ", nub->ioRead8(j, map));
		}
	    }
	    IOLog("\n");
	}
	map->release();
    }

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

    // in a real driver you should release any previously mapped windows here


    // release this device's enabler
    nub->unconfigure();

    if (workLoop && interruptSource) {
	workLoop->removeEventSource(interruptSource);
	interruptSource->release();
	interruptSource = NULL;
	workLoop = NULL;
    }

    super::stop(provider);
}

IOReturn
ApplePCCardSample::message(UInt32 type, IOService * provider, void * argument)
{
    if (type == kIOPCCardCSEventMessage) {
	IOLog("ApplePCCardSample::message, nub=%p, CS event received type=0x%x.\n", nub, (unsigned int)argument);
    }
    return 0;
}

void
ApplePCCardSample::interruptOccurred(IOInterruptEventSource * src, int i)
{
    IOLog("ApplePCCardSample::interruptOccurred, nub=%p, src=%p, i=0x%x.\n", nub, src, i);
}
