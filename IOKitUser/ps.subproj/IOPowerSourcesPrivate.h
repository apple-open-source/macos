/*
 * Copyright (c) 2003-2010 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef _IOPowerSourcesPrivate_h_
#define _IOPowerSourcesPrivate_h_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/*! 
 *  @header     IOPowerSources.h
 *  @abstract   Functions for interpreting power source info
 *  @discussion Provided as internal, publicly unsupported helper functions.  
 *
 *              Use with caution.
 */

/* kIOPSReadUserVisible and kIOPSReadAll are arguments to IOPSRequestBatteryUpdate */
enum {
    kIOPSReadSystemBoot     = 1,
    kIOPSReadAll            = 2,
    kIOPSReadUserVisible    = 4
};

/*!
 * @function    IOPSRequestBatteryUpdate
 * @abstract    Tell the battery driver to read the battery's state.
 * @discussion  OS X will automatically refresh user-visible battery state every 60 seconds.
 *              OS X will refresh non-user-visible battery state every 10 minutes, or less frequently.
 *              This API is primarily intended for diagnostic tools, that require more frequent
 *              updates.
 *              This call is asynchronous. This initiates a battery update, and caller should listen
 *              for a notification <code>@link kIOPSAnyPowerSourcesNotificationKey @/link</code>.
 * @param       type Pass kIOPSReadUserVisible to request user-visible data, namely
 *              time remaining & capacity. Pass kIOPSReadAll to request all battery data.
 * @result      kIOReturnSuccess on success; other IOReturn on failure.
 */
IOReturn        IOPSRequestBatteryUpdate(int type);


/*! 
 * @function    IOPSCopyInternalBatteriesArray
 * @abstract    Returns a CFArray of all batteries attached to the system.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no batteriess are attached to the system. A CFArray of CFTypeRef's that
 *              reference battery descriptions. De-reference each CFTypeRef member of the array
 *              using IOPSGetPowerSourceDescription.
 */
CFArrayRef      IOPSCopyInternalBatteriesArray(CFTypeRef snapshot);

/*! 
 * @function    IOPSCopyUPSArray
 * @abstract    Returns a CFArray of all UPS's attached to the system.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no UPS's are attached to the system. A CFArray of CFTypeRef's that
 *              reference UPS descriptions. De-reference each CFTypeRef member of the array
 *              using IOPSGetPowerSourceDescription.
 */
CFArrayRef      IOPSCopyUPSArray(CFTypeRef snapshot);

/*! 
 * @function    IOPSGetActiveBattery
 * @abstract    Returns the active battery.
 * @discussion  Call to determine the active battery on the system. On single battery
 *              systems this returns the 1 battery. On two battery systems this returns a reference
 *              to either battery.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no batteries are present, a CFTypeRef indicating the active battery 
 *              otherwise. You must dereference this CFTypeRef with IOPSGetPowerSourceDescription().
 */
CFTypeRef       IOPSGetActiveBattery(CFTypeRef snapshot);

/*! 
 * @function    IOPSGetActiveUPS
 * @abstract    Returns the active UPS. 
 * @discussion  You should call this to determine which UPS the system is connected to.
 *              This is trivial on single UPS systems, but on machines with multiple UPS's attached,
 *              it's important to track which one is actively providing power.
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @result      NULL if no UPS's are present, a CFTypeRef indicating the active UPS otherwise.
 *              You must dereference this CFTypeRef with IOPSGetPowerSourceDescription().
 */
CFTypeRef       IOPSGetActiveUPS(CFTypeRef snapshot);

/*! 
 * @function    IOPSPowerSourceSupported
 * @abstract    Indicates whether a power source is present on a given system.
 * @discussion  For determining if you should show UPS-specific UI
 * @param       snapshot The CFTypeRef returned by IOPSCopyPowerSourcesInfo()
 * @param       type A CFStringRef describing the type of power source (AC Power, Battery Power, UPS Power)
 * @result      kCFBooleanTrue if the power source is supported, kCFBooleanFalse otherwise.
 */
CFBooleanRef    IOPSPowerSourceSupported(CFTypeRef snapshot, CFStringRef type);

/*! 
 *  @typedef    IOPSPowerSourceID
 *  @abstract   An object of type IOPSPowerSourceID refers to a published power source. 
 *  @discussion May be NULL. The IOPSPowerSourceID contains no visible itself; it may
 *              only be passed as an argument to IOPS API.
 */
typedef struct  OpaqueIOPSPowerSourceID *IOPSPowerSourceID;

/*! 
 * @function    IOPSCreatePowerSource
 * @abstract    Call this once per publishable power source to announce the presence of the power source.
 * @discussion  This call will not make the power source visible to the clients of IOPSCopyPowerSourcesInfo();
 *              call IOPSSetPowerSourceDetails to share details.
 * @param       outPS Upon success, this parameter outPS will contain a reference to the new power source.
 *              This reference must be released with IOPSReleasePowerSource when (and if) the power source is no longer available
 *              as a power source to the OS.
 * @result      Returns kIOReturnSuccess on success, see IOReturn.h for possible failure codes.
 */
IOReturn IOPSCreatePowerSource(IOPSPowerSourceID *outPS);

/*! 
 *  @function   IOPSSetPowerSourceDetails
 *  @abstract   Call this when a managed power source's state has substantially changed,
 *              and that state should be reflected in the OS.
 *  @discussion Generally you should not call this more than once a minute - most power sources
 *              change state so slowly that once per minute is enough to provide accurate UI.
 *
 *              You may call this more frequently/immediately on any sudden changes in state,
 *              like sudden removal, or emergency low power warnings.
 *
 * @param       whichPS argument is the IOPSPowerSourceID returned from IOPSCreatePowerSource().
 *              Only the process that created this IOPSPowerSourceID may update its details.
 *
 * @param       details Caller should populate the details dictionary with information describing the power source,
 *              using dictionary keys in IOPSKeys.h
 *              Every dictionary provided here completely replaces any prior published dictionary.
 *  
 * @result      Returns kIOReturnSuccess on success, see IOReturn.h for possible failure codes.
 */
IOReturn        IOPSSetPowerSourceDetails(IOPSPowerSourceID whichPS, CFDictionaryRef details);

/*! 
 * @function    IOPSReleasePowerSource
 * @abstract    Call this when the managed power source has been physically removed from the system,
 *              or is no longer available as a power source.
 *
 * @param       whichPS The whichPS argument is the IOPSPowerSourceID returned from IOPSCreatePowerSource().
 * @result      Returns kIOReturnSuccess on success, see IOReturn.h for possible failure codes.
 */
IOReturn        IOPSReleasePowerSource(IOPSPowerSourceID whichPS);

/*!
 * @define      kIOPSNotifyPercentChange
 * @abstract    Notify(3) key. The system delivers notifications on this key when
 *              an attached power source’s percent charge remaining changes;
 *              Also delivers this notification when the active power source
 *              changes (from limited to unlimited and vice versa).
 *
 * @discussion  See API <code>@link IOPSGetPercentRemaining @/link</code> to determine the percent charge remaining;
 *              and API <code>@link IOPSDrawingUnlimitedPower @/link</code> to determine if the active power source
 *              is unlimited.
 *
 *              See also kIOPSNotifyPowerSource and kIOPSNotifyLowBattery
 */
#define kIOPSNotifyPercentChange                "com.apple.system.powersources.percent"

/*!
 * @function    IOPSGetPercentRemaining
 * @abstract    Get the percent charge remaining for the device’s power source(s).
 * @param       Returns the percent charge remaining (0 to 100).
 * @param       Returns true if the power source is being charged.
 * @param       Returns true if the power source is fully charged.
 * @result      Returns kIOReturnSuccess on success, or an error code from IOReturn.h and
 *              also report the percent remaining as 100%.
 */
IOReturn        IOPSGetPercentRemaining(int *percent, bool *isCharging, bool *isFullyCharged);

/*!
 * @function    IOPSDrawingUnlimitedPower
 * @abstract    Indicates whether the active power source is unlimited.
 * @result      Returns true if drawing from unlimited power (a wall adapter),
 *              or false if drawing from a limited source. (battery power)
 */
bool            IOPSDrawingUnlimitedPower(void);

typedef enum {
    kIOPSProvidedByAC = 1,
    kIOPSProvidedByBattery,
    kIOPSProvidedByExternalBattery
} IOPSPowerSourceIndex;
/*!
 * @function      IOPSGetSupportedPowerSources
 * Returns an integer describing which power source the system is currently 
 * drawing from.
 * Also returns true/false indicating whether the system has battery/UPS 
 * supported and/or attached.
 * This may return an error if called very early during system boot.
 *
 */
IOReturn IOPSGetSupportedPowerSources(IOPSPowerSourceIndex *active,
                                      bool *batterySupport,
                                      bool *externalBatteryAttached);


/*!
 * These bits decipher battery state stored in notify_get_state(kIOPSTimeRemainingNotificationKey)
 * For internal use only. 
 * Callers should use the public API IOPSGetTimeRemainingEstimate() to access this data.
 */
#define kPSTimeRemainingNotifyExternalBit       (1 << 16)
#define kPSTimeRemainingNotifyChargingBit       (1 << 17)
#define kPSTimeRemainingNotifyUnknownBit        (1 << 18)
#define kPSTimeRemainingNotifyValidBit          (1 << 19)
#define kPSTimeRemainingNotifyNoPollBit         (1 << 20)
#define kPSTimeRemainingNotifyFullyChargedBit   (1 << 21)
#define kPSTimeRemainingNotifyBattSupportBit    (1 << 22)
#define kPSTimeRemainingNotifyUPSSupportBit     (1 << 23)
#if TARGET_OS_IPHONE
#define kPSCriticalLevelBit                     (1 << 24)
#define kPSRestrictedLevelBit                   (1 << 25)
#endif
#define kPSTimeRemainingNotifyActivePS8BitsStarts   56

#if TARGET_OS_IPHONE
/*
 * Notify(3) string on which powerd posts a notification when system enters critical level
 */
#define kIOPSNotifyCriticalLevel            "com.apple.system.powersources.criticallevel"
/*
 * Notify(3) string on which powerd posts a notification when system enters restricted mode
 */
#define kIOPSNotifyRestrictedMode           "com.apple.system.powersources.restrictedmode"
#endif

/*!
 * @define      kIOPSBattLogEntryTime
 * @abstract    CFDictionary key used by IOPSCopyChargeLog
 * @discussion
 *              CFDate type. Specifies the time at which an log entry is made.
 */
#define kIOPSBattLogEntryTime       "Log Entry Timestamp"

/*!
 * @define      kIOPSBattLogEntryTZ
 * @abstract    CFDictionary key used by IOPSCopyChargeLog
 * @discussion
 *              CFNumber type with CFNumberType set to kCFNumberDoubleType.
 *              This value specifies difference in seconds between current system 
 *              time zone and GMT. Obtained with CFTimeZoneGetSecondsFromGMT().
 */
#define kIOPSBattLogEntryTZ         "Log Entry Timezone"


/*
 *!  @function IOPSCopyChargeLog
 *
 *   @abstract Returns an array of historical battery data collected over the past 2 hours.
 *             This records a maximum of 2 hours of history at 5 minute intervals.
 *             This charge log resets upon system boot and every time this SPI is called.
 *             Caller must be signed with the 'com.apple.private.iokit.powerlogging' entitlement.
 *             It is intended for Apple internal use only.
 *
 *   @param sinceTime   IOPSCopyChargeLog will return all power source history (if any) since this date.
 *                      This should be UTC based timestamp.
 *
 *   @param chargeLog   If successful, the dictionary returned will have the name of batteries as keys. A CFArray
 *                      is associated with each key and this array contains log entries collected since the
 *                      specified time 'sinceTime'.
 *                      Each entry in this array will be a CFDictionary.
 *                      Each dictionary will contain some or all of the following keys as defined in <IOKit/ps/IOPSKeys.h>.
 *                            - kIOPSBattLogEntryTime   - CFDate - the GMT time when the entry is recorded.
 *                            - kIOPSBattLogEntryTZ     - CFNumber double type. Specifies the difference in seconds 
 *                                                        between system's current time zone and GMT
 *                            - kIOPSCurrentCapacityKey - mAh. A CFNumber int type.
 *                            - kIOPSMaxCapacityKey     - mAh. This is the Full Charge Capacity. CFNumber int type.
 *                            - kIOPSPowerSourceStateKey   - CFString - with string kIOPSACPowerValue or kIOPSBatteryPowerValue
 *                            - kIOPSIsChargingKey      - CFBoolean - is charging or not.
 *                            - kIOPSIsChargedKey       - CFBoolean - fully charged or not
 *                            - kIOPSCurrentKey         - mA. Roughly a one minute average of the battery's amperage.
 *
 *                      Upon error, or when no history is available, IOPSCopyChargeLog shall return 
 *                      an empty array in this parameter.
 *
 *  @result             Returns kIOReturnSuccess on success. Can return kIOReturnError or 
 *                      kIOReturnNotSupported on platforms with power sources.
 */
IOReturn IOPSCopyChargeLog(CFAbsoluteTime sinceTime, CFDictionaryRef *chargeLog);


/*
 * Power adapter related internal keys
 */

/*!
 * @define      kIOPSPowerAdapterSerialStringKey
 *
 * @abstract    The power adapter's serial string.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterSerialStringKey    "SerialString"

/*!
 * @define      kIOPSPowerAdapterNameKey
 *
 * @abstract    The power adapter's name.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */

#define kIOPSPowerAdapterNameKey            "Name"

/*!
 * @define      kIOPSPowerAdapterNameKey
 *
 * @abstract    The power adapter's manufacturer's id.
 *              The value associated with this key is a CFNumber kCFNumberIntType integer value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterManufacturerIDKey  "Manufacturer"

/*!
 * @define      kIOPSPowerAdapterHardwareVersionKey
 *
 * @abstract    The power adapter's hardware version.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterHardwareVersionKey       "HwVersion"

/*!
 * @define      kIOPSPowerAdapterFirmwareVersionKey
 *
 * @abstract    The power adapter's firmware version.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterFirmwareVersionKey       "FwVersion"


__END_DECLS

#endif
