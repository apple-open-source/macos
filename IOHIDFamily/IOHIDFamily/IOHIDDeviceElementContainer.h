//
//  IOHIDDeviceElementContainer.h
//  IOHIDFamily
//
//  Created by dekom on 10/23/18.
//

#ifndef IOHIDDeviceElementContainer_h
#define IOHIDDeviceElementContainer_h

#include "IOHIDElementContainer.h"

class IOHIDDevice;

class IOHIDDeviceElementContainer : public IOHIDElementContainer
{
    OSDeclareDefaultStructors(IOHIDDeviceElementContainer)
    
private:
    struct ExpansionData {
        IOHIDDevice     *owner;
    };
    
    ExpansionData       *_reserved;
    
protected:
    virtual bool init(void *descriptor, IOByteCount length, IOHIDDevice *owner);
    virtual void free() APPLE_KEXT_OVERRIDE;
    
public:
    static IOHIDDeviceElementContainer *withDescriptor(void *descriptor,
                                                       IOByteCount length,
                                                       IOHIDDevice *owner);
    
    IOReturn updateElementValues(IOHIDElementCookie *cookies,
                                 UInt32 cookieCount = 1) APPLE_KEXT_OVERRIDE;
    
    IOReturn postElementValues(IOHIDElementCookie *cookies,
                               UInt32 cookieCount = 1) APPLE_KEXT_OVERRIDE;
};

#endif /* IOHIDDeviceElementContainer_h */
