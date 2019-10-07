//
//  AppleUserTestHIDEventDriver.h
//  IOHIDFamily
//
//  Created by dekom on 1/14/19.
//

#ifndef AppleUserTestHIDEventDriver_h
#define AppleUserTestHIDEventDriver_h

#include <IOKitUser/IOUserHIDEventDriver.iig>

class AppleUserTestHIDEventDriver : public IOUserHIDEventDriver
{
public:
    virtual kern_return_t Start(IOService * provider) override;
};

#endif /* AppleUserTestHIDEventDriver_h */
