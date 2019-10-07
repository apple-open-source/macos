/* iig generated from AppleUserTestHIDEventDriver.h */

/* AppleUserTestHIDEventDriver.h:1-12 */
//
//  AppleUserTestHIDEventDriver.h
//  IOHIDFamily
//
//  Created by dekom on 1/14/19.
//

#ifndef AppleUserTestHIDEventDriver_h
#define AppleUserTestHIDEventDriver_h

#include <HIDDriverKit/IOUserHIDEventDriver.h>  /* .iig include */ 

/* class AppleUserTestHIDEventDriver AppleUserTestHIDEventDriver.h:13-16 */


#define AppleUserTestHIDEventDriver_Start_Args \
        IOService * provider

#define AppleUserTestHIDEventDriver_Methods \
\
public:\
\
    virtual kern_return_t\
    Dispatch(const IORPC rpc) APPLE_KEXT_OVERRIDE;\
\
    static kern_return_t\
    _Dispatch(AppleUserTestHIDEventDriver * self, const IORPC rpc);\
\
\
protected:\
    /* _Impl methods */\
\
    kern_return_t\
    Start_Impl(IOService_Start_Args);\
\
\
protected:\
    /* _Invoke methods */\
\


#define AppleUserTestHIDEventDriver_KernelMethods \
\
protected:\
    /* _Impl methods */\
\


#define AppleUserTestHIDEventDriver_VirtualMethods \
\
public:\
\


#if !KERNEL

extern OSMetaClass          * gAppleUserTestHIDEventDriverMetaClass;
extern OSClassLoadInformation AppleUserTestHIDEventDriver_Class;

class AppleUserTestHIDEventDriverMetaClass : public OSMetaClass
{
public:
    virtual kern_return_t
    New(OSObject * instance) override;
    virtual kern_return_t
    Dispatch(const IORPC rpc) override;
};

#endif /* !KERNEL */

#if !KERNEL

class AppleUserTestHIDEventDriverInterface : public OSInterface
{
public:
};

struct AppleUserTestHIDEventDriver_IVars;
struct AppleUserTestHIDEventDriver_LocalIVars;

class AppleUserTestHIDEventDriver : public IOUserHIDEventDriver, public AppleUserTestHIDEventDriverInterface
{
#if !KERNEL
    friend class AppleUserTestHIDEventDriverMetaClass;
#endif /* !KERNEL */

#if !KERNEL
public:
    union
    {
        AppleUserTestHIDEventDriver_IVars * ivars;
        AppleUserTestHIDEventDriver_LocalIVars * lvars;
    };
#endif /* !KERNEL */

    using super = IOUserHIDEventDriver;

#if !KERNEL
    AppleUserTestHIDEventDriver_Methods
    AppleUserTestHIDEventDriver_VirtualMethods
#endif /* !KERNEL */

};
#endif /* !KERNEL */

/* AppleUserTestHIDEventDriver.h:18- */
#endif /* AppleUserTestHIDEventDriver_h */
