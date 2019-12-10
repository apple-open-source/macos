/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import "IOHIDUPSClass.h"
#import <IOKit/ps/IOPSKeysPrivate.h>
#import <IOKit/ps/IOPSKeys.h>
#import <IOKit/pwr_mgt/IOPM.h>
#import "IOHIDUsageTables.h"
#import "AppleHIDUsageTables.h"
#import "HIDLibElement.h"
#import <AssertMacros.h>
#import <Foundation/NSDate.h>
#import "IOHIDDeviceClass.h"
#import "IOHIDDebug.h"
#include <IOKit/hid/IOHIDPrivateKeys.h>

// See HID spec for explanation (6.2.2.7 Global Items)
#define kIOHIDUnitVolt          0x00F0D121
#define kIOHIDUnitAmp           0x00100001
#define kIOHIDUnitAmpSec        0x00101001
#define kIOHIDUnitKelvin        0x00010001

#define kIOHIDUnitExponentVolt  7

@implementation IOHIDUPSClass

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID *)outInterface
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, uuidBytes);
    HRESULT result = E_NOINTERFACE;
    
    if (CFEqual(uuid, IUnknownUUID) || CFEqual(uuid, kIOCFPlugInInterfaceID)) {
        *outInterface = &self->_plugin;
        CFRetain((__bridge CFTypeRef)self);
        result = S_OK;
    } else if (CFEqual(uuid, kIOUPSPlugInInterfaceID) ||
               CFEqual(uuid, kIOUPSPlugInInterfaceID_v140)) {
        *outInterface = (LPVOID *)&_ups;
        CFRetain((__bridge CFTypeRef)self);
        result = S_OK;
    }
    
    if (uuid) {
        CFRelease(uuid);
    }
    
    return result;
}

- (IOReturn)probe:(NSDictionary * _Nonnull __unused)properties
          service:(io_service_t)service
         outScore:(SInt32 * _Nonnull __unused)outScore
{
    if (IOObjectConformsTo(service, "IOHIDDevice")) {
        return kIOReturnSuccess;
    }
    
    return kIOReturnUnsupported;
}

- (void)parseProperties:(NSDictionary *)props
{
    /*
     * Convert our IOHID keys into IOPS keys to be read by powerd. These
     * properties are read in the getProperties method
     */
    
    _properties[@(kIOPSTransportTypeKey)] = props[@(kIOHIDTransportKey)];
    _properties[@(kIOPSNameKey)] = props[@(kIOHIDProductKey)];
    if (!_properties[@(kIOPSNameKey)]) {
        _properties[@(kIOPSNameKey)] = props[@(kIOHIDManufacturerKey)];
    }
    _properties[@(kIOPSVendorIDKey)] = props[@(kIOHIDVendorIDKey)];
    _properties[@(kIOPSProductIDKey)] = props[@(kIOHIDProductIDKey)];
    _properties[@(kIOPSAccessoryIdentifierKey)] = props[@(kIOHIDSerialNumberKey)];
    
    if ([props objectForKey:@(kIOHIDModelNumberKey)]) {
        _properties[@(kIOPSModelNumber)] = [props objectForKey:@(kIOHIDModelNumberKey)];
    }
    
    
    uint32_t usagePage = [props[@(kIOHIDPrimaryUsagePageKey)] intValue];
    uint32_t usage = [props[@(kIOHIDPrimaryUsageKey)] intValue];
    
    if (usagePage == kHIDPage_GenericDesktop &&
        usage == kHIDUsage_GD_Keyboard) {
        _properties[@(kIOPSAccessoryCategoryKey)] = @(kIOPSAccessoryCategoryKeyboard);
    } else if (props[@(kIOHIDGameControllerTypeKey)]) {
        _properties[@(kIOPSAccessoryCategoryKey)] = @(kIOPSAccessoryCategoryGameController);
    }
    
    UPSLog("properties: %@", _properties);
}

- (void)parseElements:(NSArray *)elements
{
    /*
     * Iterate through the HID elements and only keep track of the ones related
     * to UPS. Each element we're interested in will have an IOPS key associated
     * with it that we'll store. The keys are also used in conjunction with the
     * getCapabilities method to figure out what capabilities are supported by
     * the device.
     */
    
    for (id ref in elements) {
        IOHIDElementRef eleRef = (__bridge IOHIDElementRef)ref;
        HIDLibElement *element = [[HIDLibElement alloc] initWithElementRef:eleRef];
        NSMutableArray *allElements = [[NSMutableArray alloc] init];
        NSArray *matchingElements = nil;
        bool input = (element.type <= kIOHIDElementTypeInput_ScanCodes);
        bool output = (element.type == kIOHIDElementTypeOutput);
        
        switch (element.usagePage) {
            case kHIDPage_PowerDevice:
                switch (element.usage) {
                    case kHIDUsage_PD_DelayBeforeShutdown:
                        if (!input) {
                            element.psKey = @(kIOPSCommandDelayedRemovePowerKey);
                        }
                        break;
                    case kHIDUsage_PD_DelayBeforeStartup:
                        if (!input) {
                            element.psKey = @(kIOPSCommandStartupDelayKey);
                        }
                        break;
                    case kHIDUsage_PD_ConfigVoltage:
                        if (!input) {
                            element.psKey = @(kIOPSCommandSetRequiredVoltageKey);
                        }
                        break;
                    case kHIDUsage_PD_Voltage:
                        element.psKey = @(kIOPSVoltageKey);
                        break;
                    case kHIDUsage_PD_Current:
                        element.psKey = @(kIOPSCurrentKey);
                        break;
                    case kHIDUsage_PD_AudibleAlarmControl:
                        if (!input) {
                            element.psKey = @(kIOPSCommandEnableAudibleAlarmKey);
                        }
                        break;
                    case kHIDUsage_PD_Used:
                        if (!input) {
                            element.psKey = @(kIOPSAppleBatteryCaseCommandEnableChargingKey);
                        }
                        break;
                    case kHIDUsage_PD_ConfigCurrent:
                        if (input) {
                            element.psKey = @(kIOPSAppleBatteryCaseAvailableCurrentKey);
                        } else if (output) {
                            element.psKey = @(kIOPSCommandSetCurrentLimitKey);
                        }
                        break;
                    case kHIDUsage_PD_Temperature:
                        if (output) {
                            element.psKey = @(kIOPSCommandSendCurrentTemperature);
                        } else {
                            element.psKey = @(kIOPSTemperatureKey);
                        }
                        break;
                    case kHIDUsage_PD_InternalFailure:
                        element.psKey = @(kIOPSInternalFailureKey);
                        break;
                }
                break;
            case kHIDPage_BatterySystem:
                switch (element.usage) {
                    case kHIDUsage_BS_Charging:
                    case kHIDUsage_BS_Discharging:
                        element.psKey = @(kIOPSIsChargingKey);
                        break;
                    case kHIDUsage_BS_AbsoluteStateOfCharge:
                    case kHIDUsage_BS_RemainingCapacity:
                        if (output) {
                            element.psKey = @(kIOPSCommandSendCurrentStateOfCharge);
                        } else {
                            element.psKey = @(kIOPSCurrentCapacityKey);
                        }
                        break;
                    case kHIDUsage_BS_FullChargeCapacity:
                        element.psKey = @(kIOPSMaxCapacityKey);
                        break;
                    case kHIDUsage_BS_RunTimeToEmpty:
                        element.psKey = @(kIOPSTimeToEmptyKey);
                        break;
                    case kHIDUsage_BS_AverageTimeToFull:
                        element.psKey = @(kIOPSTimeToFullChargeKey);
                        break;
                    case kHIDUsage_BS_ACPresent:
                        element.psKey = @(kIOPSPowerSourceStateKey);
                        break;
                    case kHIDUsage_BS_CycleCount:
                        element.psKey = @(kIOPMPSCycleCountKey);
                        break;
                }
                break;
            case kHIDPage_AppleVendorBattery:
                switch (element.usage) {
                    case kHIDUsage_AppleVendorBattery_RawCapacity:
                        element.psKey = @(kAppleRawCurrentCapacityKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_NominalChargeCapacity:
                        element.psKey = @(kIOPSNominalCapacityKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_CumulativeCurrent:
                        element.psKey = @(kIOPSAppleBatteryCaseCumulativeCurrentKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_AdapterFamily:
                        element.psKey = @(kIOPMPSAdapterDetailsFamilyKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_Address:
                        if (output) {
                            element.psKey = @(kIOPSAppleBatteryCaseCommandSetAddress);
                        } else {
                            element.psKey = @(kIOPSAppleBatteryCaseAddress);
                        }
                        break;
                    case kHIDUsage_AppleVendorBattery_ChargingVoltage:
                        element.psKey = @(kAppleBatteryCaseChargingVoltageKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_AverageChargingCurrent:
                        if (output) {
                            element.psKey = @(kIOPSCommandSendAverageChargingCurrent);
                        }
                        break;
                    case kHIDUsage_AppleVendorBattery_IncomingVoltage:
                        element.psKey = @(kIOPSAppleBatteryCaseIncomingVoltage);
                        break;
                    case kHIDUsage_AppleVendorBattery_IncomingCurrent:
                        element.psKey = @(kIOPSAppleBatteryCaseIncomingCurrent);
                        break;
                    case kHIDUsage_AppleVendorBattery_Cell0Voltage:
                        element.psKey = @(kIOPSAppleBatteryCaseCell0Voltage);
                        break;
                    case kHIDUsage_AppleVendorBattery_Cell1Voltage:
                        element.psKey = @(kIOPSAppleBatteryCaseCell1Voltage);
                        break;
                    case kHIDUsage_AppleVendorBattery_DebugPowerStatus:
                        element.psKey = @(kIOPSDebugInformation_PowerStatusKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_DebugChargingStatus:
                        element.psKey = @(kIOPSDebugInformation_ChargingStatusKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_DebugInductiveStatus:
                        element.psKey = @(kIOPSDebugInformation_InductiveStatusKey);
                        break;
                }
                break;
            case kHIDPage_AppleVendor:
                switch (element.usage) {
                    case kHIDUsage_AppleVendor_Color:
                        element.psKey = @(kIOPSDeviceColor);
                        element.isConstant = YES;
                        break;
                }
            default:
                break;
        }
        
        if (!element.psKey) {
            continue;
        }
        
        [allElements addObjectsFromArray:_elements.input];
        [allElements addObjectsFromArray:_elements.output];
        [allElements addObjectsFromArray:_elements.feature];
        
        // Avoid adding duplicate elements
        matchingElements = [self copyElements:allElements psKey:element.psKey];
        for (HIDLibElement *match in matchingElements) {
            if (match.usagePage == element.usagePage &&
                match.usage == element.usage &&
                match.type == element.type) {
                continue;
            }
        }
        
        [_capabilities addObject:element.psKey];
        
        if (input) {
            [_elements.input addObject:element];
        } else if (output) {
            [_elements.output addObject:element];
        } else {
            [_elements.feature addObject:element];
            
            /* Constant feature elements are the one which needs to be updated only one time
             * We should have this option  propogated on hid element . This work is tracked by
             * rdar://problem/55080850 and following changes need to be revisited
             */
            
            if (element.isConstant && !element.isUpdated) {
                
                UPSLog("Feature element (UP : %x, U : %x) has const data, retrieve current value and skip polling",element.usagePage, element.usage);
                // Retrieve current value for element
                [self updateElements:@[element]];
                element.isUpdated = YES;
                continue;
            }
            
            UPSLog("Feature element (UP : %x, U : %x) added for polling", element.usagePage, element.usage);
            
            /*
             * Feature elements should be polled every 5 seconds. We create a
             * timer here and return it with the array we pass back in the
             * createAsyncEventSource method.
             */
            if (!_timer) {
                UPSLog("Create time for polling feature reports");
                
                _timer = [[NSTimer alloc] initWithFireDate:[NSDate date]
                                                  interval:5.0
                                                   repeats:YES
                                                     block:^(NSTimer *timer __unused)
                {
                    // only dispatch an event if the element values were updated.
                    if ([self updateEvent] && _eventCallback) {
                        UPSLog("timer dispatchEvent: %@", _upsEvent);
                        
                        (_eventCallback)(_eventTarget,
                                         kIOReturnSuccess,
                                         _eventRefcon,
                                         (void *)&_ups,
                                         (__bridge CFDictionaryRef)_upsEvent);
                    }
                }];
            }
        }
    }
    
    /*
     * Command elements are elements that can be updated by the sendCommand
     * method. The array consists of output and feature elements.
     */
    [_commandElements addObjectsFromArray:_elements.output];
    [_commandElements addObjectsFromArray:_elements.feature];
    
    /*
     * Event elements are elements that are received by the getElement and
     * event callback methods. The array consists of input and feature elements.
     */
    [_eventElements addObjectsFromArray:_elements.input];
    [_eventElements addObjectsFromArray:_elements.feature];
    
    UPSLog("capabilities: %@", _capabilities);
}

- (NSArray *)copyElements:(NSArray *)array psKey:(NSString *)psKey
{
    // Finds elements corresponding to their IOPS key
    NSMutableArray *result = [[NSMutableArray alloc] init];
    
    for (HIDLibElement *element in array) {
        if ([element.psKey isEqualToString:psKey]) {
            [result addObject:element];
        }
    }
    
    return result;
}

- (HIDLibElement *)latestElement:(NSArray *)array psKey:(NSString *)psKey
{
    /*
     * Returns the element with the most up to date value. Some psKeys can have
     * more than one element associated with them, so we should used the latest
     * value when updating an event.
     */
    HIDLibElement *latest = nil;
    uint64_t latestTimestamp = 0;
    NSArray *elements = [self copyElements:array psKey:psKey];
    
    for (HIDLibElement *element in elements) {
        if (element.timestamp > latestTimestamp) {
            latest = element;
            latestTimestamp = element.timestamp;
        }
    }
    
    return latest;
}

- (void)updateElements:(NSArray *)elements
{
    IOReturn ret = kIOReturnError;
    NSInteger elementsToUpdate = 0;
    
    if (!_transaction) {
        UPSLogError("Invalid transaction");
        return;
    }
    
    ret = (*_transaction)->setDirection(_transaction, kIOHIDTransactionDirectionTypeInput, 0);
    
    if (ret) {
        UPSLogError("Failed to set transaction direction %x",ret);
        return;
    }
    
    for (HIDLibElement *element in elements) {
        
        // we shouldn't query multiple times if given element is const feature element.
        if (element.isConstant && element.isUpdated) {
            UPSLog("Feature element UP : %x , U : %x updated, skip device call",element.usagePage, element.usage);
            continue;
        }
        
        ret = (*_transaction)->addElement(_transaction, element.elementRef, 0);
        if (ret) {
            UPSLog("Failed to add element to transaction %x",ret);
            continue;
        }
        elementsToUpdate++;
    }
    
    if (elementsToUpdate == 0) {
        UPSLog("Nothing to commit skip");
        (*_transaction)->clear(_transaction, 0);
        return;
    }
    
    ret = (*_transaction)->commit(_transaction, 0, 0, 0, 0);
    if (ret != kIOReturnSuccess) {
        (*_transaction)->clear(_transaction, 0);
        UPSLogError("Failed to commit input element transaction with error %x",ret);
        return;
    }
    
    for (HIDLibElement *element in elements) {
        
        IOHIDValueRef value = NULL;
        
        // we shouldn't query multiple times if given element is const feature element.
        if (element.isConstant && element.isUpdated) {
            UPSLog("Feature element UP : %x , U : %x updated, skip device call",element.usagePage, element.usage);
            continue;
        }
        
        ret = (*_transaction)->getValue(_transaction, element.elementRef, &value, 0);
        
        if (ret == kIOReturnSuccess && value) {
            element.valueRef = value;
        }
    }
    
    (*_transaction)->clear(_transaction, 0);
    return;
}

- (BOOL)updateEvent
{
    bool updated = false;
    bool isCharging = false;
    bool isDischarging = false; // Keeping this seperate bool for case when both charge / discharge is 1 (possible ??)
    bool isACSource = false;
    
    /*
     * Get the latest element values. Our input elements will already be updated
     * when we receive our valueAvailableCallback, so we just need to get the
     * feature elements.
     */
    [self updateElements:_elements.feature];
    
    for (HIDLibElement *element in _eventElements) {
        HIDLibElement *latest = [self latestElement:_eventElements
                                              psKey:element.psKey];
        
        if (latest && ![element isEqual:latest]) {
            UPSLog("Skipping duplicate element (UP : %x U : %x Type : %u IV: %ld) with key %@\n",element.usagePage, element.usage, (unsigned int)element.type, (long)element.integerValue, element.psKey);
            continue;
        }
        
        NSObject *previousValue = _upsEvent[element.psKey];
        NSObject *newValue = nil;
        NSString *elementKey = element.psKey;
        SInt32 translatedValue = (SInt32)element.integerValue;
        double exponent = element.unitExponent < 8 ? element.unitExponent :
                                                -(0x10 - element.unitExponent);
        
        switch (element.usagePage) {
            case kHIDPage_PowerDevice:
                switch (element.usage) {
                    case kHIDUsage_PD_Voltage:
                        // convert to mV
                        translatedValue *= 1000;
                        if (element.unit == kIOHIDUnitVolt) {
                            translatedValue *= pow(10, (exponent -
                                                        kIOHIDUnitExponentVolt));
                        }
                        break;
                    case kHIDUsage_PD_Current:
                    case kHIDUsage_PD_ConfigCurrent:
                        // convert to mA
                        translatedValue *= 1000;
                        if (element.unit == kIOHIDUnitAmp) {
                            translatedValue *= pow(10, exponent);
                        }
                        break;
                    case kHIDUsage_PD_Temperature:
                        // convert kelvin to degrees celsius
                        if (element.unit == kIOHIDUnitKelvin) {
                            translatedValue *= pow(10, exponent);
                            translatedValue -= 273.15;
                        }
                        break;
                    case kHIDUsage_PD_InternalFailure:
                        newValue = element.integerValue ? @TRUE : @FALSE;
                        break;
                }
                break;
            case kHIDPage_BatterySystem:
                switch (element.usage) {
                    case kHIDUsage_BS_Charging:
                        newValue = element.integerValue ? @YES : @NO;
                        isCharging |= (element.integerValue ? 1 : 0);
                        break;
                    case kHIDUsage_BS_Discharging:
                        newValue = element.integerValue ? @FALSE : @TRUE;
                        isDischarging |= (element.integerValue ? 1 : 0);
                        break;
                    case kHIDUsage_BS_AbsoluteStateOfCharge:
                    case kHIDUsage_BS_RemainingCapacity:
                        // Capacity can be given in Amp-Seconds or %
                        if (element.unit == kIOHIDUnitAmpSec) {
                            translatedValue /= 3.6;
                        }
                        
                        // If units are in %, update time to full and time to
                        // empty elements
                        if (!element.unit) {
                            NSArray *elements;
                            HIDLibElement *tmpElement;
                            SInt32 tmpValue;
                            
                            tmpValue = 100 - (SInt32)element.integerValue;
                            
                            elements = [self copyElements:_eventElements
                                                    psKey:@(kIOPSTimeToFullChargeKey)];
                            if (elements && elements.count) {
                                tmpElement = [elements objectAtIndex:0];
                                tmpValue = (UInt32)((double)tmpElement.integerValue *
                                                    ((double)tmpValue / 100.0));
                                
                                // convert seconds to minutes
                                _upsEvent[@(kIOPSTimeToFullChargeKey)] = @(tmpValue / 60);
                            }
                            
                            elements = [self copyElements:_eventElements
                                                    psKey:@(kIOPSTimeToEmptyKey)];
                            if (elements && elements.count) {
                                tmpElement = [elements objectAtIndex:0];
                                // convert seconds to minutes
                                _upsEvent[@(kIOPSTimeToEmptyKey)] = @(tmpElement.integerValue / 60);
                            }
                        }
                        break;
                    case kHIDUsage_BS_FullChargeCapacity:
                        // Capacity can be given in Amp-Seconds or %
                        if (element.unit == kIOHIDUnitAmpSec) {
                            translatedValue /= 3.6;
                        }
                        break;
                    case kHIDUsage_BS_RunTimeToEmpty:
                        // convert seconds to minutes
                        translatedValue /= 60;
                        break;
                    case kHIDUsage_BS_AverageTimeToFull: {
                        NSArray *elements;
                        HIDLibElement *tmpElement;
                        
                        elements = [self copyElements:_eventElements
                                                psKey:@(kIOPSCurrentCapacityKey)];
                        if (elements && elements.count) {
                            tmpElement = [elements objectAtIndex:0];
                            translatedValue = (UInt32)(((double)translatedValue) *
                                                       (((double)(100 - tmpElement.integerValue)) / 100.0));
                            
                            // convert seconds to minutes
                            translatedValue /= 60;
                        }
                        break;
                    }
                    case kHIDUsage_BS_ACPresent:
                        newValue = element.integerValue ? @(kIOPSACPowerValue) : @(kIOPSBatteryPowerValue);
                        isACSource |= (element.integerValue ? 1 : 0);
                        break;
                }
                break;
            case kHIDPage_AppleVendorBattery:
                switch (element.usage) {
                    case kHIDUsage_AppleVendorBattery_RawCapacity:
                    case kHIDUsage_AppleVendorBattery_NominalChargeCapacity:
                        // Capacity can be given in Amp-Seconds or %
                        if (element.unit == kIOHIDUnitAmpSec) {
                            translatedValue /= 3.6;
                        }
                        break;
                    case kHIDUsage_AppleVendorBattery_CumulativeCurrent:
                    case kHIDUsage_AppleVendorBattery_IncomingCurrent:
                        // convert to mA
                        translatedValue *= 1000;
                        if (element.unit == kIOHIDUnitAmp) {
                            translatedValue *= pow(10, exponent);
                        }
                        break;
                    case kHIDUsage_AppleVendorBattery_ChargingVoltage:
                    case kHIDUsage_AppleVendorBattery_IncomingVoltage:
                    case kHIDUsage_AppleVendorBattery_Cell0Voltage:
                    case kHIDUsage_AppleVendorBattery_Cell1Voltage:
                        // convert to mV
                        translatedValue *= 1000;
                        if (element.unit == kIOHIDUnitVolt) {
                            translatedValue *= pow(10, (exponent -
                                                        kIOHIDUnitExponentVolt));
                        }
                        break;
                    case kHIDUsage_AppleVendorBattery_Address:
                        newValue = element.dataValue;
                        break;
                    case kHIDUsage_AppleVendorBattery_DebugPowerStatus:
                        _debugInformation[@(kIOPSDebugInformation_PowerStatusKey)] = @(element.integerValue);
                        newValue = _debugInformation;
                        elementKey = @(kIOPSDebugInformationKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_DebugChargingStatus:
                        _debugInformation[@(kIOPSDebugInformation_ChargingStatusKey)] = @(element.integerValue);
                        newValue = _debugInformation;
                        elementKey = @(kIOPSDebugInformationKey);
                        break;
                    case kHIDUsage_AppleVendorBattery_DebugInductiveStatus:
                        _debugInformation[@(kIOPSDebugInformation_InductiveStatusKey)] = @(element.integerValue);
                        newValue = _debugInformation;
                        elementKey = @(kIOPSDebugInformationKey);
                        break;
                }
                break;
        }
        
        // Wrap the integer value if a new NSObject value wasn't explicitly set
        if (newValue == nil) {
            newValue = @(translatedValue);
        }
        
        updated |= ![newValue isEqual:previousValue];
        
        // if our element has a timestamp then we know its legit
        if (element.timestamp) {
            _upsEvent[elementKey] = newValue;
        }
    }
    
    // rdar://problem/57132356 , we have logic to report power state
    // based on 3 usages kHIDUsage_BS_Charging, kHIDUsage_BS_Discharging,
    // kHIDUsage_BS_ACPresent. Following is observation from Cyberpower UPS
    // device :
    // 1. When UPS power  < max power -> BS Charging is reported 1
    // 2. When UPS power == max power -> BS Charging is reported 0
    // Case 2 is persistant across reboots, once battery is fully charged
    // it will report 0 for this usage
    // 3. When UPS is unlpuged from power source , or battery consumption >
    // battery charge rate it reports Battery Discharging as 1
    // 4. AC preset usage truely convey power source
    UPSLog("Power Source status isACSource : %s , isCharging : %s , isDischarging : %s", isACSource ? "Yes" : "No", isCharging ? "Yes" : "No", isDischarging ? "Yes" : "No");
    
    if (isACSource || (isCharging && !isDischarging)) {
        _upsEvent[@(kIOPSPowerSourceStateKey)] = @(kIOPSACPowerValue);
    } else {
        _upsEvent[@(kIOPSPowerSourceStateKey)] = @(kIOPSBatteryPowerValue);
    }
    
    // When both charging and discharging are reported this should be battery (since AC is not reported in first check)
    
    return updated;
}

static void _valueAvailableCallback(void *context,
                                    IOReturn result,
                                    void *sender __unused)
{
    IOHIDUPSClass *me = (__bridge id)context;
    
    [me valueAvailableCallback:result];
}

- (void)valueAvailableCallback:(IOReturn)result
{
    // drain the queue and update our elements
    while (result == kIOReturnSuccess) {
        IOHIDValueRef value = NULL;
        result = (*_queue)->copyNextValue(_queue, &value, 0, 0);
        if (value) {
            HIDLibElement *tmp;
            HIDLibElement *element;
            NSUInteger index;
            IOHIDElementRef elementRef = IOHIDValueGetElement(value);
            
            tmp = [[HIDLibElement alloc] initWithElementRef:elementRef];
            index = [_elements.input indexOfObject:tmp];
            
            element = [_elements.input objectAtIndex:index];
            element.valueRef = value;
            
            CFRelease(value);
        }
    }
    
    [self updateEvent];
    if (_eventCallback) {
        UPSLog("dispatchEvent: %@", _upsEvent);
        
        (_eventCallback)(_eventTarget,
                         kIOReturnSuccess,
                         _eventRefcon,
                         (void *)&_ups,
                         (__bridge CFDictionaryRef)_upsEvent);
    }
}

- (IOReturn)start:(NSDictionary * _Nonnull __unused)properties
          service:(io_service_t)service
{
    IOReturn ret = kIOReturnError;
    CFMutableDictionaryRef deviceProperties = NULL;;
    CFArrayRef elements = NULL;
    IOCFPlugInInterface **plugin = NULL;
    SInt32 score = 0;
    HRESULT result = E_NOINTERFACE;
    
    ret = IORegistryEntryCreateCFProperties(service,
                                            &deviceProperties,
                                            kCFAllocatorDefault,
                                            0);
    require_action(ret == kIOReturnSuccess, exit, {
        UPSLogError("Failed to query properties with error %x",ret);
    });
    
    
    require_action(deviceProperties, exit, {
        UPSLogError("deviceProperties not valid");
        ret = kIOReturnInvalid;
    });
    
    
    [self parseProperties:(__bridge NSDictionary *)deviceProperties];
    
    ret = IOCreatePlugInInterfaceForService(service,
                                            kIOHIDDeviceTypeID,
                                            kIOCFPlugInInterfaceID,
                                            &plugin, &score);
    require_action(ret == kIOReturnSuccess , exit, {
        UPSLogError("Failed to create plugin interface with error %x",ret);
    });
    
    require_action(plugin, exit, {
        UPSLogError("Plugin not valid");
        ret = kIOReturnInvalid;
    });
    
    result = (*plugin)->QueryInterface(plugin,
                            CFUUIDGetUUIDBytes(kIOHIDDeviceDeviceInterfaceID),
                            (LPVOID *)&_device);
    require_action(result == S_OK , exit, {
        UPSLogError("Failed to get device interface with error %d",(int)result);
        ret = kIOReturnError;
    });
    
    require_action(_device, exit, {
        UPSLogError("Device not valid");
        ret = kIOReturnInvalid;
    });
    
    ret = (*_device)->open(_device, 0);
    require_action(ret == kIOReturnSuccess, exit, {
         UPSLogError("Failed to open device with error %x",ret);
    });
    
    ret = (*_device)->copyMatchingElements(_device, nil, &elements, 0);
    require_action(ret == kIOReturnSuccess, exit, {
        UPSLogError("Failed to copy matching elements with error %x",ret);
    });
    
    require_action(elements, exit, {
        UPSLogError("Elements not valid");
        ret = kIOReturnInvalid;
    });
    
   
    // setup queue
    result = (*_device)->QueryInterface(_device,
                            CFUUIDGetUUIDBytes(kIOHIDDeviceQueueInterfaceID),
                            (LPVOID *)&_queue);
    require_action(result == S_OK, exit, {
        UPSLogError("Failed to get queue interface with error %d",result);
        ret = kIOReturnError;
        
    });
    
    require_action(_queue, exit, {
        UPSLogError("Queue not valid");
        ret = kIOReturnInvalid;
    });
    
    // setup transaction
    result = (*_device)->QueryInterface(_device,
                        CFUUIDGetUUIDBytes(kIOHIDDeviceTransactionInterfaceID),
                        (LPVOID *)&_transaction);
    require_action(result == S_OK , exit, {
        UPSLogError("Failed to get transaction interface with error %d",result);
        ret = kIOReturnError;
    });
    
    require_action(_transaction, exit, {
        UPSLogError("Transaction not valid");
        ret = kIOReturnInvalid;
    });
    
    (*_transaction)->setDirection(_transaction,
                                  kIOHIDTransactionDirectionTypeOutput, 0);
    
    [self parseElements:(__bridge NSArray *)elements];
    
    
    (*_queue)->setDepth(_queue, (uint32_t)_elements.input.count, 0);
    
    for (HIDLibElement *element in _elements.input) {
        // only interested in input elements
        if (element.type <= kIOHIDElementTypeInput_ScanCodes) {
            (*_queue)->addElement(_queue, element.elementRef, 0);
        }
    }
    
    ret = (*_queue)->getAsyncEventSource(_queue, (CFTypeRef *)&_runLoopSource);
    require(ret == kIOReturnSuccess && _runLoopSource, exit);
    
    ret = (*_queue)->setValueAvailableCallback(_queue,
                                               _valueAvailableCallback,
                                               (__bridge void *)self);
    require_noerr(ret, exit);
    
    ret = (*_queue)->start(_queue, 0);
    require_noerr(ret, exit);
    
    // get the initial values for our input/feature elements
    [self updateElements:_elements.input];
    [self updateEvent];
    
    ret = kIOReturnSuccess;
    
exit:
    if (plugin) {
        (*plugin)->Release(plugin);
    }
    
    if (elements) {
        CFRelease(elements);
    }
    
    if (deviceProperties) {
        CFRelease(deviceProperties);
    }
    
    return ret;
}

- (IOReturn)stop
{
    if (_queue) {
        (*_queue)->stop(_queue, 0);
    }
    
    (*_device)->close(_device, 0);
    
    return kIOReturnSuccess;
}

static IOReturn _getProperties(void *iunknown, CFDictionaryRef *properties)
{
    IOHIDUPSClass *me = (__bridge id)((*((IUnknownVTbl**)iunknown))->_reserved);
    
    return [me getProperties:properties];
}

- (IOReturn)getProperties:(CFDictionaryRef *)properties
{
    /*
     * Returns a dictionary of properties published by the device. Properties
     * are generated from the IOHID properties and translated into IOPS
     * properties in the parseProperties method
     */
    
    if (!properties) {
        return kIOReturnBadArgument;
    }
    
    *properties = (__bridge CFDictionaryRef)_properties;
    
    return kIOReturnSuccess;
}

static IOReturn _getCapabilities(void *iunknown, CFSetRef *capabilities)
{
    IOHIDUPSClass *me = (__bridge id)((*((IUnknownVTbl**)iunknown))->_reserved);
    
    return [me getCapabilities:capabilities];
}

- (IOReturn)getCapabilities:(CFSetRef *)capabilities
{
    /*
     * Returns a set of strings that specify the capabilities of the UPS device.
     * Strings are derived from IOPSKeys.h and are generated in the
     * parseElements method.
     */
    
    if (!capabilities) {
        return kIOReturnBadArgument;
    }
    
    *capabilities = (__bridge CFSetRef)_capabilities;
    
    return kIOReturnSuccess;
}

static IOReturn _getEvent(void *iunknown, CFDictionaryRef *event)
{
    IOHIDUPSClass *me = (__bridge id)((*((IUnknownVTbl**)iunknown))->_reserved);
    
    return [me getEvent:event];
}

- (IOReturn)getEvent:(CFDictionaryRef *)event
{
    /*
     * Returns a dictionary of the current values of the UPS device. The
     * dictionary contains an IOPS key and the element's most recent value.
     * We need to translate the values returned by the elements into the values
     * expected by powerd.
     */
    
    if (!event) {
        return kIOReturnBadArgument;
    }
    
    [self updateEvent];
    *event = (__bridge CFDictionaryRef)_upsEvent;
    
    UPSLog("getEvent: %@", _upsEvent);
    
    return kIOReturnSuccess;
}

static IOReturn _setEventCallback(void *iunknown,
                                  IOUPSEventCallbackFunction callback,
                                  void *target,
                                  void *refcon)
{
    IOHIDUPSClass *me = (__bridge id)((*((IUnknownVTbl**)iunknown))->_reserved);
    
    return [me setEventCallback:callback target:target refcon:refcon];
}

- (IOReturn)setEventCallback:(IOUPSEventCallbackFunction)callback
                      target:(void *)target
                      refcon:(void *)refcon
{
    /*
     * Sets up a callback that occurs whenever a UPS HID element is updated.
     * We will execute this callback when our queue has a new element added to
     * it that we receive in our queue callback.
     */
    _eventCallback  = callback;
    _eventTarget    = target;
    _eventRefcon    = refcon;
    
    return kIOReturnSuccess;
}

static IOReturn _sendCommand(void *iunknown, CFDictionaryRef command)
{
    IOHIDUPSClass *me = (__bridge id)((*((IUnknownVTbl**)iunknown))->_reserved);
    
    return [me sendCommand:(__bridge NSDictionary *)command];
}

- (IOReturn)sendCommand:(NSDictionary *)command
{
    /*
     * Issues an element value update to the device. The command dictionary that
     * is passed in contains an IOPS key and the desired value of the element.
     * We need to translate the incoming value to the proper units before
     * sending it to the element.
     */
    UPSLog("sendCommand: %@", command);
    
    if (!command || !command.count) {
        return kIOReturnBadArgument;
    }
    
    if (!_transaction) {
        UPSLogError("Invalid transaction");
        return kIOReturnError;
    }
    
    (*_transaction)->setDirection(_transaction, kIOHIDTransactionDirectionTypeOutput, 0);
    
    [command enumerateKeysAndObjectsUsingBlock:^(NSString *key,
                                                 id value,
                                                 BOOL *stop __unused) {
        NSArray *elements = [self copyElements:_commandElements psKey:key];
        
        // find the HID element with the correct IOPS key and add it to the
        // transaction.
        for (HIDLibElement *element in elements) {
            if ([value isKindOfClass:[NSNumber class]]) {
                NSInteger val = ((NSNumber *)value).integerValue;
                
                if ([key isEqualToString:@(kIOPSCommandDelayedRemovePowerKey)] ||
                    [key isEqualToString:@(kIOPSCommandStartupDelayKey)]) {
                    // convert minutes to seconds
                    val *= 60;
                } else if ([key isEqualToString:@(kIOPSCommandEnableAudibleAlarmKey)]) {
                    val = val ? 2 : 1;
                }
                
                element.integerValue = val;
            } else if ([value isKindOfClass:[NSData class]]) {
                element.dataValue = (NSData *)value;
            } else {
                continue;
            }
            
            (*_transaction)->addElement(_transaction, element.elementRef, 0);
            
            (*_transaction)->setValue(_transaction,
                                      element.elementRef,
                                      element.valueRef, 0);
        }
    }];
    
    IOReturn ret = (*_transaction)->commit(_transaction, 0, 0, 0, 0);
    (*_transaction)->clear(_transaction, 0);
    return ret;
}

static IOReturn _createAsyncEventSource(void *iunknown, CFTypeRef *source)
{
    IOHIDUPSClass *me = (__bridge id)((*((IUnknownVTbl**)iunknown))->_reserved);
    
    return [me createAsyncEventSource:source];
}

- (IOReturn)createAsyncEventSource:(CFTypeRef *)source
{
    /*
     * Returns a CFArrayRef of event sources; the timer that is used for polling
     * feature elements and the run loop source associated with the element
     * queue.
     */
    
    NSMutableArray *eventSources = [[NSMutableArray alloc] init];
    
    if (_timer) {
        CFRetain((CFTypeRef)_timer);
        [eventSources addObject:_timer];
    }
    
    if (_runLoopSource) {
        CFRetain(_runLoopSource);
        [eventSources addObject:(__bridge id)_runLoopSource];
    }
    
    *source = (CFArrayRef)CFBridgingRetain(eventSources);
    return kIOReturnSuccess;
}

- (instancetype)init
{
    self = [super init];
    
    if (!self) {
        return nil;
    }
    
    _ups = (IOUPSPlugInInterface_v140 *)malloc(sizeof(*_ups));
    
    *_ups = (IOUPSPlugInInterface_v140) {
        // IUNKNOWN_C_GUTS
        ._reserved = (__bridge void *)self,
        .QueryInterface = self->_vtbl->QueryInterface,
        .AddRef = self->_vtbl->AddRef,
        .Release = self->_vtbl->Release,
        
        // IOUPSPlugInInterface_v140
        .getProperties = _getProperties,
        .getCapabilities = _getCapabilities,
        .getEvent = _getEvent,
        .setEventCallback = _setEventCallback,
        .sendCommand = _sendCommand,
        .createAsyncEventSource = _createAsyncEventSource
    };
    
    _properties = [[NSMutableDictionary alloc] init];
    _capabilities = [[NSMutableSet alloc] init];
    _elements.input = [[NSMutableArray alloc] init];
    _elements.output = [[NSMutableArray alloc] init];
    _elements.feature = [[NSMutableArray alloc] init];
    _commandElements = [[NSMutableArray alloc] init];
    _eventElements = [[NSMutableArray alloc] init];
    _upsEvent = [[NSMutableDictionary alloc] init];
    _debugInformation = [[NSMutableDictionary alloc] init];
    
    return self;
}

- (void)dealloc
{
    free(_ups);
    
    if (_queue) {
        (*_queue)->Release(_queue);
    }
    
    if (_transaction) {
        (*_transaction)->Release(_transaction);
    }
    
    if (_device) {
        (*_device)->Release(_device);
    }
}

@end
