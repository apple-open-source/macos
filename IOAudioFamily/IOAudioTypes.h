/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_IOAUDIOTYPES_H
#define _IOKIT_IOAUDIOTYPES_H

#include <libkern/OSTypes.h>
#include <mach/message.h>
// also need an include for AbsoluteTime

/*!
 * @enum IOAudioDMAEngineMemory
 * @abstract Used to identify the type of memory requested by a client process to be mapped into its process space
 * @discussion This is the parameter to the type field of IOMapMemory when called on an IOAudioDMAEngine.  This is
 *  only intended for use by the Audio Device API library.
 * @constant kSampleBuffer This requests the IOAudioDMAEngine's sample buffer
 * @constant kStatus This requests the IOAudioDMAEngine's status buffer.  It's type is IOAudioDMAEngineStatus.
 * @constant kMixBuffer This requests the IOAudioDMAEngine's mix buffer
*/
typedef enum _IOAudioDMAEngineMemory {
    kStatusBuffer = 0,
    kSampleBuffer = 1,
    kMixBuffer = 2
} IOAudioDMAEngineMemory;

/*!
 * @enum IOAudioDMAEngineCalls
 * @abstract The set of constants passed to IOAudioDMAEngineUserClient::getExternalMethodForIndex() when making calls
 *  from the IOAudioFamily user client code.
 */
typedef enum _IOAudioDMAEngineCalls {
    kAudioDMAEngineCallRegisterClientBuffer = 0,
    kAudioDMAEngineCallUnregisterClientBuffer = 1,
    kAudioDMAEngineCallGetConnectionID = 2
} IOAudioDMAEngineCalls;

typedef enum _IOAudioDMAEngineTraps {
    kAudioDMAEngineTrapPerformClientIO = 0
} IOAudioDMAEngineTraps;

/*! @defined IOAUDIODMAENGINE_NUM_CALLS The number of elements in the IOAudioDMAEngineCalls enum. */
#define IOAUDIODMAENGINE_NUM_CALLS		3

typedef enum _IOAudioDMAEngineNotifications {
    kAudioDMAEngineStreamFormatChangeNotification = 0
} IOAudioDMAEngineNotifications;

/*!
 * @typedef IOAudioDMAEngineStatus
 * @abstract Shared-memory structure giving DMA engine status
 * @discussion
 * @field fVersion Indicates version of this structure
 * @field fCurrentLoopCount Number of times around the ring buffer since the DMA engine started
 * @field fLastLoopTime Timestamp of the last time the ring buffer wrapped
 * @field fEraseHeadSampleFrame Location of the erase head in sample frames - erased up to but not
 *        including the given sample frame
 */

typedef struct _IOAudioDMAEngineStatus {
    UInt32					fVersion;
    volatile UInt32			fCurrentLoopCount;
    volatile AbsoluteTime	fLastLoopTime;
    volatile UInt32			fEraseHeadSampleFrame;
} IOAudioDMAEngineStatus;

#define CURRENT_IOAUDIODMAENGINE_STATUS_STRUCT_VERSION	2

typedef struct _IOAudioStreamFormat {
    UInt32	fNumChannels;
    UInt32	fSampleFormat;
    UInt32	fNumericRepresentation;
    UInt8	fBitDepth;
    UInt8	fBitWidth;
    UInt8	fAlignment;
    UInt8	fByteOrder;
    UInt8	fIsMixable;
} IOAudioStreamFormat;


/*!
 * @defined IOAUDIODMAENGINE_DEFAULT_MIX_BUFFER_SAMPLE_SIZE
 */

#define IOAUDIODMAENGINE_DEFAULT_MIX_BUFFER_SAMPLE_SIZE	sizeof(float)

/* The following are for use only by the IOKit.framework audio family code */

/*!
 * @enum IOAudioControlCalls
 * @abstract The set of constants passed to IOAudioControlUserClient::getExternalMethodForIndex() when making calls
 *  from the IOAudioFamily user client code.
 * @constant kAudioControlSetValue Used to set the value of an IOAudioControl.
 * @constant kAudioControlGetValue Used to get the value of an IOAudioControl.
 */
typedef enum _IOAudioControlCalls {
    kAudioControlSetValue = 0,
    kAudioControlGetValue = 1
} IOAudioControlCalls;

/*! @defined IOAUDIOCONTROL_NUM_CALLS The number of elements in the IOAudioControlCalls enum. */
#define IOAUDIOCONTROL_NUM_CALLS 	2

/*!
 * @enum IOAudioControlNotifications
 * @abstract The set of constants passed in the type field of IOAudioControlUserClient::registerNotificaitonPort().
 * @constant kAudioControlValueChangeNotification Used to request value change notifications.
 */
typedef enum _IOAudioControlNotifications {
    kAudioControlValueChangeNotification = 0
} IOAudioControlNotifications;

/*!
 * @struct IOAudioNotificationMessage
 * @abstract Used in the mach message for IOAudio notifications.
 * @field messageHeader Standard mach message header
 * @field ref The param passed to registerNotificationPort() in refCon.
 */
typedef struct _IOAudioNotificationMessage
{
    mach_msg_header_t	messageHeader;
    UInt32		type;
    UInt32		ref;
} IOAudioNotificationMessage;

typedef struct _IOAudioSampleRate
{
    UInt32	whole;
    UInt32	fraction;
} IOAudioSampleRate;



#endif /* _IOKIT_IOAUDIOTYPES_H */
