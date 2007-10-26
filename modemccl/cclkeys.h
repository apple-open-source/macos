/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
// *****************************************************************************
//  cclkeys.h
//
//  Created by kevine on 3/1/06.
//  Recreated by sspies 15 January 2007
// *****************************************************************************


#ifndef CCLKEYS_H
#define CCLKEYS_H

#include <SystemConfiguration/SCSchemaDefinitions.h>

// CCL bundles ...
#define kCCLFileExtension       CFSTR("ccl")

// top-level structure for bundle dictionary
#define kCCLPersonalitiesKey    CFSTR("CCL Personalities")      // dict
// personality names are keys; -> kSCPropNetModemConnectionPersonality
#define kCCLDefaultPersonalityKey CFSTR("Default Personality")  // dict
#define kCCLVersionKey          CFSTR("CCL Version")            // integer
#define kCCLBundleVersion       1

// Personality's type
#define kCCLConnectTypeKey      CFSTR("Connect Type")       // string
#define kCCLConnectGPRS             CFSTR("GPRS")
#define kCCLConnectDialup           CFSTR("Dialup")

// flat scripts that this personality obsoletes
#define kCCLSupersedesKey    CFSTR("Supersedes")            // array of str

// How personality is described in the UI
#define kCCLDeviceNamesKey      CFSTR("Device Names")       // array
#define kCCLVendorKey  kSCPropNetModemDeviceVendor /*("DeviceVendor")*/ // str
#define kCCLModelKey   kSCPropNetModemDeviceModel  /*("DeviceModel")*/  // str

// Device capabilities assumed by personality
#define kCCLGPRSCapabilitiesKey CFSTR("GPRS Capabilities")  // dict
#define kCCLSupportsCIDQueryKey     CFSTR("CID Query")         // bool
#define kCCLSupportsDataModeKey     CFSTR("Data Mode")         //bool(AT+CGDATA)
#define kCCLSupportsDialModeKey     CFSTR("Dial Mode")         // bool (ATD *99)
#define kCCLMaximumCIDKey           CFSTR("Maximum CID")       // integer
#define kCCLIndependentCIDs         CFSTR("Independent CIDs")  // bool
#define kCCLIndependentCIDsKey      CFSTR("Independent CIDs")  // bool
// Independent CIDs means that commands like AT+CGDCONT= won't override
// APN valumes stored (by CID) in the device.

// Parameters passed to the script for this personality
#define kCCLScriptNameKey       CFSTR("Script Name")        // in Resources/
#define kCCLParametersKey       CFSTR("CCLParameters")      // dict
#define kCCLConnectSpeedKey         CFSTR("Connect Speed")     // string (^20)
#define kCCLInitStringKey           CFSTR("Init String")       // string (^21)
#define kCCLPreferredAPNKey         CFSTR("Preferred APN")     // str (-> ^22)
#define kCCLPreferredCIDKey         CFSTR("Preferred CID")     // int (-> ^23)
// Preferred CID w/o Preferred APN means use APN stored "at" that CID in phone
// A Preferred CID with Preferred APN means assign the given APN to said CID

// varStrings 23-26 reserved for future language-defined arguments

// Four script-defined arguments to be used as seen fit
#define kCCLVarString27Key          CFSTR("varString 27")   // string (^27)
#define kCCLVarString28Key          CFSTR("varString 28")   // string (^28)
#define kCCLVarString29Key          CFSTR("varString 29")   // string (^29)
#define kCCLVarString30Key          CFSTR("varString 30")   // string (^30)

// traditional argument now in the dict passed from pppd to CCLEngine
#define kModemPhoneNumberKey    CFSTR("Phone Number")   // string (^1 & ^7-9)    


// CCLEngine control keys
#define kCCLEngineDictKey           CFSTR("Engine Control") // control dict

// engine control parameters
#define kCCLEngineModeKey               CFSTR("Mode")               // str
#define kCCLEngineModeConnect           CFSTR("Connect")
#define kCCLEngineModeDisconnect        CFSTR("Disconnect")
#define kCCLEngineBundlePathKey         CFSTR("Bundle Path")        // str
#define kCCLEngineServiceIDKey          CFSTR("Service ID")         // str
#define kCCLEngineAlertNameKey          CFSTR("Alert Name")         // str
#define kCCLEngineIconPathKey           CFSTR("Icon Path")          // str
#define kCCLEngineCancelNameKey         CFSTR("Cancel Name")        // str
#define kCCLEngineSyslogLevelKey        CFSTR("Syslog Level")       // int
#define kCCLEngineSyslogFacilityKey     CFSTR("Syslog Facility")    // int
#define kCCLEngineVerboseLoggingKey     CFSTR("Verbose Logging")    // int
#define kCCLEngineLogToStdErrKey        CFSTR("Log To Stderr")      // int

// #define kCCLEngineLogFileKey         CFSTR("Log File")           // unused
// #define kCCLEngineBundleIconURLKey      CFSTR("BundleIconURL")   // unused

#endif      // CCLKEYS_H
