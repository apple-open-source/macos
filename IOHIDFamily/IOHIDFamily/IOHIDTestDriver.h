
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include "IOHIDDevice.h"

class IOHIDTestDriver : public IOHIDDevice
{
    OSDeclareDefaultStructors( IOHIDTestDriver )

private:
    IOWorkLoop *          _workLoop;
    IOTimerEventSource *  _timerSource;

public:
    void issueFakeReport();

    virtual void free();

    virtual bool handleStart( IOService * provider );
    virtual void handleStop(  IOService * provider );

    virtual IOReturn newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const;

    virtual OSString * newTransportString() const;
    virtual OSString * newManufacturerString() const;
    virtual OSString * newProductString() const;
    virtual OSNumber * newVendorIDNumber() const;
    virtual OSNumber * newProductIDNumber() const;
    virtual OSNumber * newVersionNumber() const;
    virtual OSNumber * newSerialNumber() const;
    virtual OSNumber * newPrimaryUsageNumber() const;
    virtual OSNumber * newPrimaryUsagePageNumber() const;
};
