//
//  IOHIDActionQueueTestDriver.h
//  IOHIDFamily
//
//  Created by dekom on 3/14/18.
//

#ifndef IOHIDActionQueueTestDriver_h
#define IOHIDActionQueueTestDriver_h

#include <IOKit/hid/IOHIDActionQueue.h>
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>

class IOHIDActionQueueTestDriver: public IOService
{
    OSDeclareDefaultStructors(IOHIDActionQueueTestDriver)
private:
    IOWorkLoop              *_actionWL;
    IOHIDActionQueue        *_actionQueue;
    
    UInt32                  _actionCounter;
    
    IOWorkLoop              *_testWL1;
    IOInterruptEventSource  *_interrupt1;
    UInt32                  _actionCounter1;
    
    IOWorkLoop              *_testWL2;
    IOInterruptEventSource  *_interrupt2;
    UInt32                  _actionCounter2;
    
    void interruptAction1(IOInterruptEventSource *sender, int count);
    void interruptAction2(IOInterruptEventSource *sender, int count);
    
    void cancelHandlerCall();
    void runTest();
    
public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
};



#endif /* IOHIDActionQueueTestDriver_h */
