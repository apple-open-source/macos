/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _WWANSCHEMADEFINITIONS_H
#define _WWANSCHEMADEFINITIONS_H

//@DynamicDictionary:
#define kWWAN_DynamicDictonary		"WWANDynamicProperties"
#define kWWAN_HardwareDictionary	"WWANHardwareProperties"

#define kWWAN_DeviceModemOverrides	"DeviceModemOverrides"
#define kWWAN_DevicePPPOverrides	"DevicePPPOverrides"

#define kWWAN_TYPE				"WWAN_TYPE"			//e.g "GSM, "CDMA"
#define kWWAN_NETWORK_NAME		"NETWORK_NAME"
#define kWWAN_RSSI				"RSSI"
#define kWWAN_SERVICE_STATE		"SERVICE_STATE"

// @HardwareDictionary: //Common to Both CDMA and GSM
#define kWWAN_FIRMWARE_VERSION	"FIRMWARE_VERSION"
#define kWWAN_HW_VERSION		"ModemSW" //"HW_VERSION"
#define kWWAN_ROAMING			"ROAMING"
#define kWWAN_PHONE_NUMBER		"PHONE_NUMBER"

//GSM Specific Properties
#define kWWAN_IMEI				"IMEI"
#define kWWAN_IMSI				"IMSI"

//@HardwareDictionary(CDMA):
//CDMA Hardware Specific Properties
#define kWWAN_PRL				"PRL"
#define kWWAN_ERI				"ERI"
#define kWWAN_ESN				"ESN"

#define kWWAN_GSM_TYPE			"GSM"
#define kWWAN_CDMA_TYPE			"CDMA"
#define kWWAN_WCDMA_TYPE		"WCDMA"

#define kWWAN_SC_SETUP			"Initializing"
#define kWWAN_UNIQUIFIER		"UniqueIdentifier"

#define kCONNECT_STATE          "CONNECT_STATE"
#define kCONNECT                "CONNECT"
#define kCONNECTED              "CONNECTED"

#define kDIS_CONNECT            "DISCONNECT"
#define kDIS_CONNECTED          "DISCONNECTED"


typedef enum {
	WWAN_DICTIONARY_UNKNOWN			= 0,
	WWAN_SET_DYNAMIC_DICTIONARY		= 1,
	WWAN_SET_HARDWARE_DICTIONARY	= 2,
	WWAN_SET_MODEM_DICTIONARY		= 3,
	WWAN_SET_PPP_DICTIONARY			= 4,
	WWAN_DEVICE_PROPERTY			= 5
}WWAN_DICTIONARY;


#endif /* _WWANSCHEMADEFINITIONS_H */
