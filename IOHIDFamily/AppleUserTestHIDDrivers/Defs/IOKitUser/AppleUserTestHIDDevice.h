#ifndef _APPLEUSERTESTHIDDRIVERS_APPLEUSERTESTHIDEVICE_H
#define _APPLEUSERTESTHIDDRIVERS_APPLEUSERTESTHIDEVICE_H

#include <IOKitUser/OSAction.h>
#include <IOKitUser/IOService.h>
#include <IOKitUser/IOUserHIDDevice.iig>
#include <IOKitUser/IOHIDInterface.iig>

class AppleUserTestHIDDevice : public IOUserHIDDevice
{

public:
    
    virtual bool init() override;

    virtual void free() override;

    virtual kern_return_t Start(IOService * provider) override;

    virtual kern_return_t Stop(IOService * provider) override;

    virtual kern_return_t getReport(IOMemoryDescriptor * report,
                                    IOHIDReportType      reportType,
                                    IOOptionBits         options,
                                    uint32_t             completionTimeout,
                                    OSAction            * action = 0) override;
    
    virtual kern_return_t setReport(IOMemoryDescriptor  * report,
                                    IOHIDReportType     reportType,
                                    IOOptionBits        options,
                                    uint32_t            completionTimeout,
                                    OSAction            * action = 0) override;

protected:
    
    virtual OSDictionary * newDeviceDescription () override;
    
    virtual OSData * newReportDescriptor ()  override;
    
    virtual void HandleReportCallback  (uint64_t                    timestamp,
                                        IOMemoryDescriptor *        report,
                                        IOHIDReportType             type,
                                        uint32_t                    reportID,
                                        OSAction *                  action) TYPE(IOHIDInterface::HandleReportCallback);
  
};

#endif /* ! _APPLEUSERTESTHIDDRIVERS_APPLEUSERTESTHIDEVICE_H */
