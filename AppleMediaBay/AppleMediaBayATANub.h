#ifndef _APPLEMEDIABAYATANUB_H_
#define _APPLEMEDIABAYATANUB_H_

#include <IOKit/platform/AppleMacIODevice.h>

// The header with the media bay:
#include "AppleMediaBay.h"

// The pourpose of this object is only to match on the ATA interfaces
// that belong to to the MediaBay before the MediaBay itself.

class AppleMediaBayATANub : public AppleMacIODevice
{
    OSDeclareDefaultStructors(AppleMediaBayATANub)
    
private:
    AppleMediaBay    *effectiveProvider;
    AppleMacIO       *effectiveMacIO;
    AppleMacIODevice *parentObject;

    // upon start this merges the property table of the caller with my
    // own one:
    bool setCorrectPropertyTable();

    // Finds the media bay in the device tree from the given point:
    IORegistryEntry *findDTMediaBay(IOService *fromHere);
    
public:
    // The usual generic methods:
    virtual IOService *probe( IOService * provider, SInt32 * score );
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);

    // The power managment method that makes sure that
    // we will be child of the MediaBay:
    virtual bool initForPM (IOService *provider);

    // we need to override these to make sure that the nub behaves correctly
    virtual bool compareName( OSString * name, OSString ** matched = 0 ) const;
    virtual IOReturn getResources( void );

    // The methods to handle the insertion and removal of ATA devices:
    virtual void handleDeviceInsertion();
    virtual void handleDeviceEjection();
};
#endif //_APPLEMEDIABAYATANUB_H_

