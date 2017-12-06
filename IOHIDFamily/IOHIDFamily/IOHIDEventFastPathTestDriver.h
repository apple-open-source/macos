/* add your code here */

#include <IOKit/hidevent/IOHIDEventDriver.h>

class IOHIDEvent;

/*! @class IOHIDEventFastPathDriver : public IOHIDEventDriver
 @abstract    driver to validate fast path copy event clients
 @discussion  driver to validate fast path copy event clients
 */

class IOHIDEventFastPathDriver: public IOHIDEventDriver
{
    OSDeclareDefaultStructors( IOHIDEventFastPathDriver )
    OSDictionary *fastClients;

protected:
    
    virtual void dispatchEvent(IOHIDEvent * event, IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
    
public:
    
    virtual bool handleStart( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual IOHIDEvent * copyEventForClient (OSObject * copySpec, IOOptionBits options, void * clientContext) APPLE_KEXT_OVERRIDE;
    virtual OSObject * copyPropertyForClient (const char * aKey, void * clientContext) const APPLE_KEXT_OVERRIDE;
    virtual IOReturn  setPropertiesForClient (OSObject * properties, void * clientContext) APPLE_KEXT_OVERRIDE;
    virtual bool openForClient (IOService * client, IOOptionBits options, OSDictionary *property, void ** clientContext) APPLE_KEXT_OVERRIDE;
    virtual void closeForClient(IOService *client, void *context, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
};
