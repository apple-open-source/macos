/*
 * Copyright (c) 2022 Apple Computer, Inc. All rights reserved.
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

/*!
    @header IOPMAssertionCategories.h

    IOPMAssertionCategories.h lays out a typedef enum that contains known category values.
    These are intended for use as possible values for the kIOPMAssertionCategoryKey key in
    the assertion info dictionary.

    These values will be used by the power management demon to associate additional properties
    that inform various run-time policies. The policies aim to better manage the user experience vs system
    performance trade-off.
 */

#ifndef _IOPMASSERTIONCATEGORIES_H_
#define _IOPMASSERTIONCATEGORIES_H_

/* Assertion Category types */
typedef enum  {
    // Slots 0 through 99 are reserved for internal use.
    kIOPMAssertionCategory_InternalGeneric = 0,

    // Slots 100 through 149 are reserved for location services
    kIOPMAssertionCategory_LocationServices_AbsoluteAltimetryGPS = 100,
    kIOPMAssertionCategory_LocationServices_AbsoluteAltimetryWiFi = 101,
    kIOPMAssertionCategory_LocationServices_LocationAwarenessProvider = 102,
    kIOPMAssertionCategory_LocationServices_HTTPDownload = 103,
    kIOPMAssertionCategory_LocationServices_BarometerAltimetry = 104,
    kIOPMAssertionCategory_LocationServices_RhythmicGnss = 105,
    kIOPMAssertionCategory_LocationServices_LocationStreaming = 106,
    kIOPMAssertionCategory_LocationServices_MotionAlarmNotifier = 107,
    kIOPMAssertionCategory_LocationServices_AvengerScanning = 108,
    kIOPMAssertionCategory_LocationServices_Submersion = 109,
    kIOPMAssertionCategory_LocationServices_DeepSubmersion = 110,
    kIOPMAssertionCategory_LocationServices_SubmersionSurfaceCalibration = 111,
    kIOPMAssertionCategory_LocationServices_NILR = 112, // NILR: Network Initiated Location Request

    // Slots 150 through 199 are reserved for networking services
    kIOPMAssertionCategory_NetworkingServices_NSURLSessionTask = 150,
    
    // Slots 200 through 299 are reserved for push services
    kIOPMAssertionCategory_PushServices_WaitForMessages = 200,
    kIOPMAssertionCategory_PushServices_SleepWakeTransition = 201,
    kIOPMAssertionCategory_PushServices_ReceivingData = 202,
    kIOPMAssertionCategory_PushServices_DelayedKeepAlive = 203,
    kIOPMAssertionCategory_PushServices_CriticalReliability = 204,
    kIOPMAssertionCategory_PushServices_RequestKeepAliveProxy = 205,
    kIOPMAssertionCategory_PushServices_OutgoingMessage = 206,
    kIOPMAssertionCategory_PushServices_Login = 207,
    kIOPMAssertionCategory_PushServices_WakeStateChange = 208,
    kIOPMAssertionCategory_PushServices_ChangingTopics = 209,
    kIOPMAssertionCategory_PushServices_ClientInitialTopicsCheckIn = 210,
    kIOPMAssertionCategory_PushServices_ClientInitialConnection = 211,
    kIOPMAssertionCategory_PushServices_LastAssertionLinger = 212,
    kIOPMAssertionCategory_PushServices_EstablishConnection = 213,
    kIOPMAssertionCategory_PushServices_OutgoingQueue = 214,
    kIOPMAssertionCategory_PushServices_WaitForKeepAliveResponse = 215,
    
    // Slots 300 through 399 are reserved for audio HAL services
    kIOPMAssertionCategory_AudioHAL_HapticsPrewarm = 300,
    kIOPMAssertionCategory_AudioHAL_Haptics = 301,
    kIOPMAssertionCategory_AudioHAL_SpeechDetectionPrewarm = 302,
    kIOPMAssertionCategory_AudioHAL_SpeechDetection = 303,
    kIOPMAssertionCategory_AudioHAL_Hawking = 304,
    kIOPMAssertionCategory_AudioHAL_LiquidDetection = 305,
    kIOPMAssertionCategory_AudioHAL_MediaStreaming = 306,
    kIOPMAssertionCategory_AudioHAL_MediaRecording = 307,
} IOPMAssertionCategory;


#endif
