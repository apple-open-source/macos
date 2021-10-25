/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2018-2020 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLEUSERHIDEVENTSERVICE_H
#define _APPLEUSERHIDEVENTSERVICE_H

#include <IOKit/hidevent/IOHIDEventService.h>
#include <IOKit/hidevent/IOHIDEventDriver.h>
#include <sys/queue.h>

struct EventCopyCaller;
struct SetPropertiesCaller;
struct SetLEDCaller;

class AppleUserHIDEventService: public IOHIDEventDriver
{
    OSDeclareDefaultStructors (AppleUserHIDEventService)

private:    
  
    STAILQ_HEAD(EventCopyCallerList, EventCopyCaller);
    STAILQ_HEAD(SetPropertiesCallerList, SetPropertiesCaller);
    STAILQ_HEAD(SetLEDCallerList, SetLEDCaller);

    struct AppleUserHIDEventService_IVars
    {
        OSArray               * elements;
        IOHIDInterface        * provider;
        uint32_t                state;
        IOCommandGate         * commandGate;
        IOWorkLoop            * workLoop;
        EventCopyCallerList     eventCopyCallers;
        SetPropertiesCallerList setPropertiesCallers;
        SetLEDCallerList        setLEDCallers;
        OSAction              * eventCopyAction;
        OSAction              * setPropertiesAction;
        OSAction              * setLEDAction;
    };
    
    AppleUserHIDEventService_IVars  *ivar;

protected:

    virtual void    completeCopyEvent(OSAction * action, IOHIDEvent * event, uint64_t context) APPLE_KEXT_OVERRIDE;

public:
    
    virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
    virtual bool init(OSDictionary * dictionary = 0) APPLE_KEXT_OVERRIDE;
    virtual void free(void) APPLE_KEXT_OVERRIDE;

    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual void handleStop(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual bool terminate(IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
    
    // IOHIDEventService overrides
    virtual IOReturn setElementValue(UInt32 usagePage,
                                     UInt32 usage,
                                     UInt32 value) APPLE_KEXT_OVERRIDE;
    virtual OSArray *getReportElements(void) APPLE_KEXT_OVERRIDE;
    virtual bool handleStart(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual OSString *getTransport(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32 getLocationID(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32 getVendorID(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32 getVendorIDSource(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32 getProductID(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32 getVersion(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32 getCountryCode(void) APPLE_KEXT_OVERRIDE;
    virtual OSString *getManufacturer(void) APPLE_KEXT_OVERRIDE;
    virtual OSString *getProduct(void) APPLE_KEXT_OVERRIDE;
    virtual OSString *getSerialNumber(void) APPLE_KEXT_OVERRIDE;
    
    
    virtual void     dispatchKeyboardEvent(AbsoluteTime                timeStamp,
                                           UInt32                      usagePage,
                                           UInt32                      usage,
                                           UInt32                      value,
                                           IOOptionBits                options = 0) APPLE_KEXT_OVERRIDE;
    
    virtual void     dispatchScrollWheelEventWithFixed(AbsoluteTime                timeStamp,
                                                       IOFixed                     deltaAxis1,
                                                       IOFixed                     deltaAxis2,
                                                       IOFixed                     deltaAxis3,
                                                       IOOptionBits                options = 0) APPLE_KEXT_OVERRIDE;
    
    virtual void    dispatchEvent(IOHIDEvent * event, IOOptionBits options=0) APPLE_KEXT_OVERRIDE;
    
    virtual IOHIDEvent *    copyEvent(
                                IOHIDEventType              type,
                                IOHIDEvent *                matching = 0,
                                IOOptionBits                options = 0) APPLE_KEXT_OVERRIDE;
    
    virtual IOHIDEvent *copyMatchingEvent(OSDictionary *matching) APPLE_KEXT_OVERRIDE;

    virtual IOReturn setProperties(OSObject * properties) APPLE_KEXT_OVERRIDE;
    virtual void completeSetProperties(OSAction * action, IOReturn status, uint64_t context) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setSystemProperties(OSDictionary * properties) APPLE_KEXT_OVERRIDE;
    virtual void completeSetLED(OSAction * action, IOReturn status, uint64_t context) APPLE_KEXT_OVERRIDE;

private:
    void updateElementsProperty(OSArray * userElements, OSArray * deviceElements);
    void setSensorProperties(OSDictionary * sensorProps, OSArray * deviceElements);
    void setDigitizerProperties(OSDictionary * digitizerProps, OSArray * deviceElements);
    void setUnicodeProperties(OSDictionary * unicodeProps, OSArray * deviceElements);
};
#endif /* !_APPLEUSERHIDEVENTSERVICE_H */
