/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2008 Apple, Inc.  All Rights Reserved.
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


#ifndef _APPLEUSERHIDDEVICE_H
#define _APPLEUSERHIDDEVICE_H

/*
 * Kernel
 */

#include <IOKit/hid/IOHIDDevice.h>
#include <sys/queue.h>


/*!
 @class AppleUserHIDDevice
 @abstract
 */
class AppleUserHIDDevice : public IOHIDDevice
{
    OSDeclareDefaultStructors (AppleUserHIDDevice);
    
private:
    
    struct AppleUserHIDDevice_IVars
    {
        IOCommandGate             * commandGate;
        IOWorkLoop                * workLoop;
        OSSet                     * syncActions;
        OSSet                     * asyncActions;
        IOLock                    * asyncActionsLock;
        uint32_t                  state;
        uint32_t                  requestTimeout;
        uint32_t                  setReportCount;
        uint32_t                  setReportFailCount;
        uint32_t                  setReportTimeoutCount;
        uint64_t                  setReportTime;
        uint32_t                  getReportCount;
        uint32_t                  getReportFailCount;
        uint32_t                  getReportTimeoutCount;
        uint64_t                  getReportTime;
        uint32_t                  inputReportCount;
        uint64_t                  inputReportTime;
    };

    AppleUserHIDDevice_IVars   * ivar;
    
    bool serializeDebugState(void *ref, OSSerialize *serializer);
    
protected:
    /*!
     @function free
     @abstract Free the IOHIDDevice object.
     @discussion Release all resources that were previously allocated,
     then call super::free() to propagate the call to our superclass.
     */
    virtual void free(void) APPLE_KEXT_OVERRIDE;
    
    /*!
     @function handleStart
     @abstract Prepare the hardware and driver to support I/O operations.
     @discussion IOHIDDevice will call this method from start() before
     any I/O operations are issued to the concrete subclass. Methods
     such as newReportDescriptor() are only called after handleStart()
     has returned true. A subclass that overrides this method should
     begin its implementation by calling the version in super, and
     then check the return value.
     @param provider The provider argument passed to start().
     @result True on success, or false otherwise. Returning false will
     cause start() to fail and return false.
     */
    virtual bool handleStart(IOService * provider) APPLE_KEXT_OVERRIDE;

    /*!
     @function handleStop
     @abstract Quiesce the hardware and stop the driver.
     @discussion IOHIDDevice will call this method from stop() to
     signal that the hardware should be quiesced and the driver stopped.
     A subclass that overrides this method should end its implementation
     by calling the version in super.
     @param provider The provider argument passed to stop().
     */
    virtual void handleStop(IOService * provider) APPLE_KEXT_OVERRIDE;
    
    /*!
     @function completeReport
     @abstract complete reports for DriverKit drivers
     @discussion This method only used by DriverKit driver
     */

    virtual void completeReport(OSAction * action, IOReturn status, uint32_t actualByteCount) APPLE_KEXT_OVERRIDE;


    /*!
     @function processReport
     @abstract process Set/Get reports for DriverKit drivers
     @discussion This method only used by DriverKit driver
     */

    IOReturn processReport(HIDReportCommandType command,
                           IOMemoryDescriptor * report,
                           IOHIDReportType      reportType,
                           IOOptionBits         options,
                           uint32_t             completionTimeout,
                           IOHIDCompletion    * completion = 0);

public:

    bool setProperty(const OSSymbol* aKey, OSObject* anObject) APPLE_KEXT_OVERRIDE;
    
    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;

    virtual void stop(IOService * provider); APPLE_KEXT_OVERRIDE;

    virtual IOService * probe(IOService * provider, SInt32 * score) APPLE_KEXT_OVERRIDE;

    virtual bool willTerminate( IOService * provider, IOOptionBits options ) APPLE_KEXT_OVERRIDE;
    
    /*!
     @function init
     @abstract Initialize an IOUserHIDDevice object.
     @discussion Prime the IOUserHIDDevice object and prepare it to support
     a probe() or a start() call. This implementation will simply call
     super::init().
     @param dictionary A dictionary associated with this IOHIDDevice
     instance.
     @result True on sucess, or false otherwise.
     */
    
    virtual bool init( OSDictionary * dictionary = 0 ) APPLE_KEXT_OVERRIDE;

    /*!
     @function newReportDescriptor
     @abstract Create and return a new memory descriptor that describes the
     report descriptor for the HID device.
     @result kIOReturnSuccess on success, or an error return otherwise.
     */
    
    virtual IOReturn newReportDescriptor(IOMemoryDescriptor ** descriptor ) const APPLE_KEXT_OVERRIDE;
    
    /*!
     @function newTransportString
     @abstract Returns a string object that describes the transport
     layer used by the HID device.
     @result A string object. The caller must decrement the retain count
     on the object returned.
     */
    
    virtual OSString * newTransportString() const APPLE_KEXT_OVERRIDE;
    
    /*!
     @function newManufacturerString
     @abstract Returns a string object that describes the manufacturer
     of the HID device.
     @result A string object. The caller must decrement the retain count
     on the object returned.
     */
    virtual OSString * newManufacturerString() const APPLE_KEXT_OVERRIDE;
    
    /*!
     @function newProductString
     @abstract Returns a string object that describes the product
     of the HID device.
     @result A string object. The caller must decrement the retain count
     on the object returned.
     */
    virtual OSString * newProductString() const APPLE_KEXT_OVERRIDE;
    
    /*! @function newVendorIDNumber
     @abstract Returns a number object that describes the vendor ID
     of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSNumber * newVendorIDNumber() const APPLE_KEXT_OVERRIDE;
    
    /*! @function newProductIDNumber
     @abstract Returns a number object that describes the product ID
     of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSNumber * newProductIDNumber() const APPLE_KEXT_OVERRIDE;
    
    /*! @function newVersionNumber
     @abstract Returns a number object that describes the version number
     of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSNumber * newVersionNumber() const APPLE_KEXT_OVERRIDE;
    
    /*! @function newSerialNumberString
     @abstract Returns a string object that describes the serial number
     of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSString * newSerialNumberString(void) const APPLE_KEXT_OVERRIDE;
    
    /*! @function newVendorIDSourceNumber
     @abstract Returns a number object that describes the vendor ID
     source of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSNumber * newVendorIDSourceNumber(void) const APPLE_KEXT_OVERRIDE;
    
    /*! @function newCountryCodeNumber
     @abstract Returns a number object that describes the country code
     of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSNumber * newCountryCodeNumber(void) const APPLE_KEXT_OVERRIDE;
    
    /*! @function newReportIntervalNumber
     @abstract Returns a number object that describes the report interval
     of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSNumber * newReportIntervalNumber(void) const APPLE_KEXT_OVERRIDE;
    
    /*! @function newLocationIDNumber
     @abstract Returns a number object that describes the location
     of the HID device.
     @result A number object. The caller must decrement the retain count
     on the object returned. */
    virtual OSNumber *newLocationIDNumber(void) const APPLE_KEXT_OVERRIDE;
    
  
    /*! @function setProperties
     *   @abstract Optionally supported external method to set properties in a registry entry.
     *   @discussion This method is not implemented by IORegistryEntry, but is available to kernel and non-kernel clients to set properties in a registry entry. IOUserClient provides connection based, more controlled access to this functionality and may be more appropriate for many uses, since there is no differentiation between clients available to this method.
     *   @param properties Any OSObject subclass, to be interpreted by the implementing method - for example an OSDictionary, OSData etc. may all be appropriate.
     *   @result An IOReturn code to be returned to the caller. */

    virtual IOReturn setProperties(OSObject * properties) APPLE_KEXT_OVERRIDE;
    
    
    /*! @function getReport
     @abstract Get a report from the HID device.
     @param report A memory descriptor that describes the memory to store
     the report read from the HID device.
     @param reportType The report type.
     @param options The lower 8 bits will represent the Report ID.  The
     other 24 bits are options to specify the request.
     @param completionTimeout Specifies an amount of time (in ms) after which
     the command will be aborted if the entire command has not been completed.
     @param completion Function to call when request completes. If omitted then
     getReport() executes synchronously, blocking until the request is complete.
     @result kIOReturnSuccess on success, or an error return otherwise. */
    
    virtual IOReturn getReport(IOMemoryDescriptor   * report,
                               IOHIDReportType      reportType,
                               IOOptionBits         options,
                               UInt32               completionTimeout,
                               IOHIDCompletion      * completion = 0) APPLE_KEXT_OVERRIDE;
    
    /*! @function setReport
     @abstract Send a report to the HID device.
     @param report A memory descriptor that describes the report to send
     to the HID device.
     @param reportType The report type.
     @param options The lower 8 bits will represent the Report ID.  The
     other 24 bits are options to specify the request.
     @param completionTimeout Specifies an amount of time (in ms) after which
     the command will be aborted if the entire command has not been completed.
     @param completion Function to call when request completes. If omitted then
     setReport() executes synchronously, blocking until the request is complete.
     @result kIOReturnSuccess on success, or an error return otherwise. */
    
    virtual IOReturn setReport(IOMemoryDescriptor   * report,
                               IOHIDReportType      reportType,
                               IOOptionBits         options,
                               UInt32               completionTimeout,
                               IOHIDCompletion      * completion = 0) APPLE_KEXT_OVERRIDE;

    /*! @function handleReportWithTime
    @abstract Handle an asynchronous report received from the HID device.
    @param timeStamp The timestamp of report.
    @param report A memory descriptor that describes the report.
    @param reportType The type of report. Currently, only
    kIOHIDReportTypeInput report type is handled.
    @param options Options to specify the request. No options are
    currently defined, and the default value is 0.
    @result kIOReturnSuccess on success, or an error return otherwise. */
    
    virtual IOReturn handleReportWithTime(
                     AbsoluteTime         timeStamp,
                     IOMemoryDescriptor * report,
                     IOHIDReportType      reportType = kIOHIDReportTypeInput,
                     IOOptionBits         options    = 0) APPLE_KEXT_OVERRIDE;
};


#endif /* !_APPLEUSERHIDDEVICE_H */


