#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/platform/AppleMacIO.h>

extern "C" {
#include <pexpert/pexpert.h>
}

#include "AppleMediaBay.h"
#include "AppleMediaBayATANub.h"

// The following define switches the way the media bay expects to communicate
// with its childs. if "TEMINATE_SERVICES" is defined a "terminate" is applied
// to the child, otherwise we simply send a message.
// #define TEMINATE_SERVICES

// If "TEMINATE_SERVICES" is not defined the following header provides us with
// the message constants:
#ifndef TEMINATE_SERVICES
#include <IOKit/ata/IOATATypes.h>
#endif //TEMINATE_SERVICES

// Define the SuperClass
#define super AppleMacIODevice

OSDefineMetaClassAndStructors(AppleMediaBayATANub, AppleMacIODevice)

// --------------------------------------------------------------------------
//
// Method: setCorrectPropertyTable
//                       
// Purpose:
//          copies all the properties of our provider into our property
//          table:
bool
AppleMediaBayATANub::setCorrectPropertyTable()
{
    if (parentObject == NULL) {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails because we miss the correct parentObject\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        return false;
    }

    // Why two methods to copy the properties ? Well because the second one
    // (the short and easy to understand one) should work, but it does not.
    // so I have to resort to a more complex, but working one.
#if 1
    // gets the dictionary with the properties of our parent object
    // (the one we try to mimic):
    OSDictionary *dict = parentObject->dictionaryWithProperties();
    if (dict != NULL) {
        OSSymbol * sym;
        OSCollectionIterator * iter;

        // Creates an iterator to explore all the dictionary.
        iter = OSCollectionIterator::withCollection(dict);
        if (iter != NULL) {
            // For all the object of the parentObject property table..
            while ( (sym = (OSSymbol *)iter->getNextObject()) ) {
                OSObject * obj;
                
                // get the object with the right symbol
                obj = dict->getObject(sym);

#ifdef APPLEMB_VERBOSE
                IOLog("AppleMediaBayATANub::%s adding property %s\n", __FUNCTION__, sym->getCStringNoCopy());
#endif // APPLEMB_VERBOSE

                // and attach it to my property table:
                setProperty(sym, obj);
            }
            
            // at this point we do not need the iterator anymore:
            iter->release();
    
            // I can assume that 
            return true;
	}
    }
#else
    // Gets the current dictionary of the object we want to mimic
    // and ouw own one (myPropertyTable).
    OSDictionary *currentDictionary = parentObject->dictionaryWithProperties();
    OSDictionary *myPropertyTable = getPropertyTable();

    if ((currentDictionary != NULL) && (myPropertyTable != NULL)) {
        // If they are not empty, merges mine with the given one
        myPropertyTable->merge(currentDictionary);
        
        // and reset my property table with the new dictionary:
        setPropertyTable(myPropertyTable);
        
        // If everything went fine returns true:
        return true;
    }
#endif

    // If we are here something went wrong in the process.
    return false;
}


// --------------------------------------------------------------------------
//
// Method: findDTMediaBay
//                       
// Purpose:
//          Finds the media bay in the device tree from the given point:
IORegistryEntry*
AppleMediaBayATANub::findDTMediaBay(IOService *fromHere)
{
    // Is the parent of our provider a media-bay in the device tree?
    IORegistryEntry *parentSymbol = fromHere->getParentEntry(gIODTPlane);
    if (parentSymbol == NULL)
        return NULL;
    else {
        const char *parentName = parentSymbol->getName();
        if ((parentName == NULL) || (strcmp(parentName, "media-bay")))
            return NULL;
    }
    
    return parentSymbol;
}

// --------------------------------------------------------------------------
//
// Method: probe
//                       
// Purpose:
//          returns a probe count the most important note is that this probe
//          must return a probe count much much higer than the one returned
//          by the ata controller, so I'll be sure that this class will
//          grab the ata node before the ATA controller:
IOService*
AppleMediaBayATANub::probe(IOService *provider, SInt32 * score)
{
    // check with the "super"
   if (super::probe( provider, score) == NULL)
	return NULL;
    
    // First checks that we are not attempting to match on ourself:
    if (OSDynamicCast(AppleMediaBayATANub, provider) != NULL)
        return NULL;

    // Is the parent of our provider a media-bay in the device tree?
    if (findDTMediaBay(provider) == NULL)
        return NULL;
    
#ifdef APPLEMB_VERBOSE
    IOLog("AppleMediaBayATANub::%s return success\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE

    // O.K. this is the right node we want to mach on since:
    // 1] the provider is not of our type.
    // 2] the provider's provider is the media-bay
    
    // so increase the score:
    *score = 10000;
    
    return this;
}

// --------------------------------------------------------------------------
//
// Method: start
//                       
// Purpose:
//         sets up all the internal variables so that it knows its position in
//         the tree, after waits until the media-bay shows up in the tree and
//         registers with it.

bool
AppleMediaBayATANub::start(IOService *provider)
{
    IOService *tmp;
    
    if (!super::start(provider))
        return false;

    // First finds the mac-io we belong to recursing up the tree:
    for (effectiveMacIO = NULL, tmp = provider;
         (tmp != NULL) && (effectiveMacIO == NULL);
         tmp = tmp->getProvider()) {
            effectiveMacIO = OSDynamicCast(AppleMacIO ,tmp);
    }
    
    // if we did not find anything is really bad, fail the start:
    if (effectiveMacIO == NULL) {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to get effectiveMacIO\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        return false;
    }
#ifdef APPLEMB_VERBOSE
    else
        IOLog("AppleMediaBayATANub::%s found effectiveMacIO:%s\n", __FUNCTION__, effectiveMacIO->getName());
#endif // APPLEMB_VERBOSE
    
    // remembers who is our provider:
    parentObject = OSDynamicCast(AppleMacIODevice, provider);
    if (parentObject == NULL)  {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to get the parent AppleMacIODevice\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        return false;
    }
#ifdef APPLEMB_VERBOSE
    else
        IOLog("AppleMediaBayATANub::%s found parentObject:%s\n", __FUNCTION__, parentObject->getName());
#endif // APPLEMB_VERBOSE

    // Makes sure that we look exactly like our provider (by copying the property table):
    if (!setCorrectPropertyTable())  {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to set the correct property table\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        return false;
    }

    // We can not just "wait" for the media bay to show up because we may have
    // 2 media bay (as in the PowerBookG3 1998). so we have to find the media bay:
    IORegistryEntry *mediaBayDT = findDTMediaBay(provider);
    if (mediaBayDT == NULL) {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to find a media bay entry\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE

        return false;
    }
    else {
        IORegistryEntry *mediaBayDriver = NULL;

        while (mediaBayDriver == NULL) {
            // Gets an iterator for all the media bay devices in the machine
            OSIterator * iter = getMatchingServices(serviceMatching("AppleMediaBay"));
            if (iter) {
#ifdef APPLEMB_VERBOSE
                int i = 0;
#endif // APPLEMB_VERBOSE
                OSObject *tmpObject;
                
                // Since we have an iterator this means that we also have at least one
                // media bay device. So Itreates until it finds the right one:
                while (((tmpObject = iter->getNextObject()) != NULL) && (mediaBayDriver == NULL)) {
                    // This should also be an IOService, but just in case we
                    // better do a Dynamic cast.
                    IOService *tmpService = OSDynamicCast(IOService, tmpObject);
                    
#ifdef APPLEMB_VERBOSE
                    if (tmpService == NULL)
                        IOLog("AppleMediaBayATANub::%s iterator %d is not IOService\n", __FUNCTION__, i++);
                    else
                        IOLog("AppleMediaBayATANub::%s iterator %d parent is 0x%08lx ours is 0x%08lx\n", __FUNCTION__,
                                                i++, (UInt32)tmpService->getProvider(), (UInt32)mediaBayDT);
#endif // APPLEMB_VERBOSE
                    // The right one is the one that has as provider the same
                    // entry we have for the media-bay devicee tree object:
                    if ((tmpService != NULL) && (tmpService->getProvider() == mediaBayDT))
                        mediaBayDriver = tmpService;
                }

                // We do not need the iterator anymore:
                iter->release();
            }
            else {
#ifdef APPLEMB_VERBOSE
                IOLog("AppleMediaBayATANub::%s fails to get the correct iteraator for the AppleMediaBay\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE

                // Waits a little for the media bya to s how up:
                IOSleep(1000);
            }
        }
        
        effectiveProvider = OSDynamicCast(AppleMediaBay, mediaBayDriver);
    }

    if (effectiveProvider == NULL)  {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to get the correct AppleMediaBay\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        return false;
    }

    // and finally registers with the mediabay:
    if (effectiveProvider->registerMediaNub(this) != kIOReturnSuccess)   {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to register with the media-bay\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        return false;
    }

    // attaches ourself at the end of the PM tree so we create a bridge between our children
    // and the media bay.
    initForPM(effectiveProvider);

#ifdef APPLEMB_VERBOSE
    IOLog("AppleMediaBayATANub::%s return success\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE

    // done and happy:
    return true;
}


// --------------------------------------------------------------------------
//
// Method: stop
//                       
// Purpose:
//          the only important thing to do is to deregisters with the media
//          bay driver. It also must emoulate an ejection since it is going away.
void 
AppleMediaBayATANub::stop(IOService *provider)
{
    if (effectiveProvider->deRegisterMediaNub(this) != kIOReturnSuccess) {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to de-register with the media-bay\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
    }
    
    // behaves as the device was removed:
    handleDeviceEjection();
}
    
// --------------------------------------------------------------------------
//
// Method: initForPM
//                       
// Purpose:
//          sets up the conditions for the power managment. This driver does not
//          do any power managment at all. It just exists in the power managment
//          tree.
bool
AppleMediaBayATANub::initForPM(IOService *provider)
{
    
    PMinit();                   // initialize superclass variables
    provider->joinPMtree(this); // attach into the power management hierarchy

    // were we able to init the power manager for this driver ? 
    if (pm_vars == NULL)
        return false;

    #define number_of_power_states 2
    static IOPMPowerState ourPowerStates[number_of_power_states] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
    };

    // register ourselves with ourself as policy-maker        
    registerPowerDriver(this, ourPowerStates, number_of_power_states);

    return true;
}
    
// --------------------------------------------------------------------------
//
// Method: compareName
//                       
// Purpose: 
// 	   Compares the name of the entry with one or more names, and optionally
//         returns the matching name. We need to override this because the parent's
//         one makes the (wrong) assumption that all MacIODevices are direct childs
//         of MacIO.
bool
AppleMediaBayATANub::compareName( OSString * name, OSString ** matched) const
{
    return effectiveMacIO->compareNubName( this, name, matched );
}

// --------------------------------------------------------------------------
//
// Method: compareName
//                       
// Purpose: 
//          Allocate any needed resources for a published IOService before
//          clients attach. We need to override this because the parent's
//         one makes the (wrong) assumption that all MacIODevices are direct childs
//         of MacIO.
IOReturn
AppleMediaBayATANub::getResources( void )
{
    return effectiveMacIO->getNubResources( this );
}

// --------------------------------------------------------------------------
//
// Method: handleDeviceInsertion
//                       
// Purpose: 
//          it is called upon insertion of a new device. If this object has
//          already a child it does not do anything. But if it does not have
//          a child it calls registerService to start matching.
void
AppleMediaBayATANub::handleDeviceInsertion()
{
    if (getChildEntry(gIOServicePlane) == NULL)
    	registerService();
    else  {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to handleDeviceInsertion because nub has already a child\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
    }
}

// --------------------------------------------------------------------------
//
// Method: handleDeviceInsertion
//                       
// Purpose: 
void
AppleMediaBayATANub::handleDeviceEjection()
{
    // First of all, did someone match on us ? We know it looking 
    IORegistryEntry *myTempchild = getChildEntry(gIOServicePlane);

#ifdef APPLEMB_VERBOSE
    IOLog("AppleMediaBayATANub::%s handleDeviceEjection\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
    
    // It mUst be an IOService to be terminated:
    IOService *ioServiceChild = OSDynamicCast(IOService, myTempchild);
    
    if (ioServiceChild != NULL) {
    
#ifdef TEMINATE_SERVICES

	if (ioServiceChild->terminate(kIOServiceSynchronous) == false)  {
#ifdef APPLEMB_VERBOSE
            IOLog("AppleMediaBayATANub::%s fails to handleDeviceEjection because nub does not have ioservice children\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        }

#else // ! TEMINATE_SERVICES

        if (messageClient(kATARemovedEvent, ioServiceChild, (void*)OSString::withCString( kATAMediaBaySocketString ), 0) != kIOReturnSuccess)  {
#ifdef APPLEMB_VERBOSE
            IOLog("AppleMediaBayATANub::%s fails to handleDeviceEjection because nub does not have ioservice children\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
        }

#endif // TEMINATE_SERVICES

    }
    else  {
#ifdef APPLEMB_VERBOSE
        IOLog("AppleMediaBayATANub::%s fails to handleDeviceEjection because nub does not have ioservice children\n", __FUNCTION__);
#endif // APPLEMB_VERBOSE
    }
}
