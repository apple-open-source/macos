/* iig generated from AppleUserTestHIDDevice.h */

/* AppleUserTestHIDDevice.h:1-8 */
#ifndef _APPLEUSERTESTHIDDRIVERS_APPLEUSERTESTHIDEVICE_H
#define _APPLEUSERTESTHIDDRIVERS_APPLEUSERTESTHIDEVICE_H

#include <IOKitUser/OSAction.h>
#include <IOKitUser/IOService.h>
#include <HIDDriverKit/IOUserHIDDevice.h>  /* .iig include */ 
#include <HIDDriverKit/IOHIDInterface.h>  /* .iig include */ 

/* class AppleUserTestHIDDevice AppleUserTestHIDDevice.h:9-45 */

#define AppleUserTestHIDDevice_HandleReportCallback_ID            0x430d750204b1a3c9ULL

#define AppleUserTestHIDDevice_Start_Args \
        IOService * provider

#define AppleUserTestHIDDevice_Stop_Args \
        IOService * provider

#define AppleUserTestHIDDevice_HandleReportCallback_Args \
        uint64_t timestamp, \
        IOMemoryDescriptor * report, \
        IOHIDReportType type, \
        uint32_t reportID, \
        OSAction * action

#define AppleUserTestHIDDevice_Methods \
\
public:\
\
    virtual kern_return_t\
    Dispatch(const IORPC rpc) APPLE_KEXT_OVERRIDE;\
\
    static kern_return_t\
    _Dispatch(AppleUserTestHIDDevice * self, const IORPC rpc);\
\
    void\
    HandleReportCallback(\
        uint64_t timestamp,\
        IOMemoryDescriptor * report,\
        IOHIDReportType type,\
        uint32_t reportID,\
        OSAction * action,\
        OSDispatchMethod supermethod = NULL);\
\
\
protected:\
    /* _Impl methods */\
\
    kern_return_t\
    Start_Impl(IOService_Start_Args);\
\
    kern_return_t\
    Stop_Impl(IOService_Stop_Args);\
\
    void\
    HandleReportCallback_Impl(AppleUserTestHIDDevice_HandleReportCallback_Args);\
\
\
protected:\
    /* _Invoke methods */\
\
    kern_return_t\
    HandleReportCallback_Invoke(const IORPC rpc,\
        void (AppleUserTestHIDDevice::*func)(AppleUserTestHIDDevice_HandleReportCallback_Args));\
\


#define AppleUserTestHIDDevice_KernelMethods \
\
protected:\
    /* _Impl methods */\
\


#define AppleUserTestHIDDevice_VirtualMethods \
\
public:\
\
    virtual bool\
    init(\
) APPLE_KEXT_OVERRIDE;\
\
    virtual void\
    free(\
) APPLE_KEXT_OVERRIDE;\
\
    virtual kern_return_t\
    getReport(\
        IOMemoryDescriptor * report,\
        IOHIDReportType reportType,\
        IOOptionBits options,\
        uint32_t completionTimeout,\
        OSAction * action) APPLE_KEXT_OVERRIDE;\
\
    virtual kern_return_t\
    setReport(\
        IOMemoryDescriptor * report,\
        IOHIDReportType reportType,\
        IOOptionBits options,\
        uint32_t completionTimeout,\
        OSAction * action) APPLE_KEXT_OVERRIDE;\
\
    virtual OSDictionary *\
    newDeviceDescription(\
) APPLE_KEXT_OVERRIDE;\
\
    virtual OSData *\
    newReportDescriptor(\
) APPLE_KEXT_OVERRIDE;\
\


#if !KERNEL

extern OSMetaClass          * gAppleUserTestHIDDeviceMetaClass;
extern OSClassLoadInformation AppleUserTestHIDDevice_Class;

class AppleUserTestHIDDeviceMetaClass : public OSMetaClass
{
public:
    virtual kern_return_t
    New(OSObject * instance) override;
    virtual kern_return_t
    Dispatch(const IORPC rpc) override;
};

#endif /* !KERNEL */

#if !KERNEL

class AppleUserTestHIDDeviceInterface : public OSInterface
{
public:
};

struct AppleUserTestHIDDevice_IVars;
struct AppleUserTestHIDDevice_LocalIVars;

class AppleUserTestHIDDevice : public IOUserHIDDevice, public AppleUserTestHIDDeviceInterface
{
#if !KERNEL
    friend class AppleUserTestHIDDeviceMetaClass;
#endif /* !KERNEL */

#if !KERNEL
public:
    union
    {
        AppleUserTestHIDDevice_IVars * ivars;
        AppleUserTestHIDDevice_LocalIVars * lvars;
    };
#endif /* !KERNEL */

    using super = IOUserHIDDevice;

#if !KERNEL
    AppleUserTestHIDDevice_Methods
    AppleUserTestHIDDevice_VirtualMethods
#endif /* !KERNEL */

};
#endif /* !KERNEL */

/* AppleUserTestHIDDevice.h:47- */
#endif /* ! _APPLEUSERTESTHIDDRIVERS_APPLEUSERTESTHIDEVICE_H */
