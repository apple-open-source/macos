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

#ifndef _IOAUDIODEFINES_H
#define _IOAUDIODEFINES_H

#define IOAUDIODEVICE_CLASS_NAME	"IOAudioDevice"
#define IOAUDIODMAENGINE_CLASS_NAME	"IOAudioDMAEngine"
#define IOAUDIOSTREAM_CLASS_NAME	"IOAudioStream"
#define IOAUDIOPORT_CLASS_NAME		"IOAudioPort"
#define IOAUDIOCONTROL_CLASS_NAME	"IOAudioControl"

/*!
 * @defined IOAUDIO_SAMPLE_RATE_KEY
 * @abstract The key in the IORegistry for the IOAudioDMAEngine sample rate attribute
 * @discussion This value is represented as an integer in samples per second.
 */
#define IOAUDIO_SAMPLE_RATE_KEY	"IOAudioSampleRate"

#define IOAUDIO_SAMPLE_RATE_WHOLE_NUMBER_KEY	"IOAudioSampleRateWholeNumber"
#define IOAUDIO_SAMPLE_RATE_FRACTION_KEY		"IOAudioSampleRateFraction"



/******
 *
 * IOAudioDevice  defines
 *
 *****/


/*!
 * @defined IOAUDIODEVICE_NAME_KEY
 * @abstract The key in the IORegistry for the IOAudioDevice name attribute.
 */
#define IOAUDIODEVICE_NAME_KEY	"IOAudioDeviceName"

/*!
 * @defined IOAUDIODEVICE_MANUFACTURER_NAME_KEY
 * @abstract The key in the IORegistry for the IOAudioDevice manufacturer name attribute.
 */
#define IOAUDIODEVICE_MANUFACTURER_NAME_KEY	"IOAudioDeviceManufacturerName"



/*****
 *
 * IOAudioDMAEngine defines
 *
 *****/


 /*!
 * @defined IOAUDIODMAENGINE_STATE_KEY
 * @abstract The key in the IORegistry for the IOAudioDMAEngine state atrribute
 * @discussion The value for this key may be one of: "Running", "Stopped" or "Paused".  Currently the "Paused"
 *  state is unimplemented.
 */
#define IOAUDIODMAENGINE_STATE_KEY		"IOAudioDMAEngineState"

/*!
 * @defined IOAUDIODMAENGINE_STATE_RUNNING
 * @abstract The value for the IOAUDIODMAENGINE_STATE_KEY in the IORegistry representing a running IOAudioDMAEngine.
 */
#define IOAUDIODMAENGINE_STATE_RUNNING	1

/*!
 * @defined IOAUDIODMAENGINE_STATE_STOPPED
 * @abstract The value for the IOAUDIODMAENGINE_STATE_KEY in the IORegistry representing a stopped IOAudioDMAEngine.
 */
#define IOAUDIODMAENGINE_STATE_STOPPED	0

/*!
 * @defined IOAUDIODMAENGINE_SAMPLE_LATENCY_KEY
 * @abstract The key in the IORegistry for the IOAudioDMAEngine sample latency key
 * @discussion This value is stored as an integer representing the number of samples from the
 *  I/O engine head to the point at which its safe to read from or write to the sample buffer.
 */
#define IOAUDIODMAENGINE_SAMPLE_LATENCY_KEY	"IOAudioDMAEngineSampleLatency"

#define IOAUDIODMAENGINE_NUM_SAMPLE_FRAMES_PER_BUFFER_KEY	"IOAudioDMAEngineNumSampleFramesPerBuffer"



/*****
 *
 * IOAudioStream defines
 *
 *****/
 
 
 #define IOAUDIOSTREAM_ID_KEY			"IOAudioStreamID"
#define IOAUDIOSTREAM_NUM_CLIENTS_KEY	"IOAudioStreamNumClients"

/*!
 * @defined IOAUDIOSTREAM_DIRECTION_KEY
 * @abstract The key in the IORegistry for the IOAudioStream direction attribute.
 * @discussion The value for this key may be either "Output" or "Input".
 */
#define IOAUDIOSTREAM_DIRECTION_KEY		"IOAudioStreamDirection"

/*!
 * @defined IOAUDIOSTREAM_DIRECTION_OUTPUT
 * @abstract The value for the IOAUDIOSTREAM_DIRECTION_KEY in the IORegistry representing an output buffer.
 */
#define IOAUDIOSTREAM_DIRECTION_OUTPUT	0

/*!
 * @defined IOAUDIOSTREAM_DIRECTION_INPUT
 * @abstract The value for the IOAUDIOSTREAM_DIRECTION_KEY in the IORegistry representing an input buffer
 */
#define IOAUDIOSTREAM_DIRECTION_INPUT	1

#define IOAUDIOSTREAM_FORMAT_KEY			"IOAudioStreamFormat"
#define IOAUDIOSTREAM_AVAILABLE_FORMATS_KEY	"IOAudioStreamAvailableFormats"

#define IOAUDIOSTREAM_NUM_CHANNELS_KEY		"IOAudioStreamNumChannels"
#define IOAUDIOSTREAM_SAMPLE_FORMAT_KEY		"IOAudioStreamSampleFormat"

#define IOAUDIOSTREAM_SAMPLE_FORMAT_LINEAR_PCM		'lpcm'

#define IOAUDIOSTREAM_NUMERIC_REPRESENTATION_KEY	"IOAudioStreamNumericRepresentation"

#define IOAUDIOSTREAM_NUMERIC_REPRESENTATION_SIGNED_INT		'sint'
#define IOAUDIOSTREAM_NUMERIC_REPRESENTATION_UNSIGNED_INT	'uint'
// Need float format(s) here

#define IOAUDIOSTREAM_BIT_DEPTH_KEY		"IOAudioStreamBitDepth"
#define IOAUDIOSTREAM_BIT_WIDTH_KEY		"IOAudioStreamBitWidth"

#define IOAUDIOSTREAM_ALIGNMENT_KEY		"IOAudioStreamAlignment"
#define IOAUDIOSTREAM_ALIGNMENT_LOW_BYTE	0
#define IOAUDIOSTREAM_ALIGNMENT_HIGH_BYTE	1

#define IOAUDIOSTREAM_BYTE_ORDER_KEY		"IOAudioStreamByteOrder"
#define IOAUDIOSTREAM_BYTE_ORDER_BIG_ENDIAN	0
#define IOAUDIOSTREAM_BYTE_ORDER_LITTLE_ENDIAN	1

#define IOAUDIOSTREAM_IS_MIXABLE_KEY		"IOAudioStreamIsMixable"

#define IOAUDIOSTREAM_MINIMUM_SAMPLE_RATE_KEY	"IOAudioStreamMinimumSampleRate"
#define IOAUDIOSTREAM_MAXIMUM_SAMPLE_RATE_KEY	"IOAudioStreamMaximumSampleRate"



/*****
 *
 * IOAudioPort defines
 *
 *****/
 
 
 /*!
 * @defined IOAUDIOPORT_TYPE_KEY
 * @abstract The key in the IORegistry for the IOAudioPort type attribute.
 * @discussion This is a driver-defined text attribute that may contain any type.
 *  Common types are defined as: "Speaker", "Headphones", "Microphone", "CD", "Line", "Digital", "Mixer", "PassThru".
 */
#define IOAUDIOPORT_TYPE_KEY		"IOAudioPortType"

/*!
 * @defined IOAUDIOPORT_TYPE_SPEAKER
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a speaker port
 */
#define IOAUDIOPORT_TYPE_SPEAKER	"Speaker"

/*!
 * @defined IOAUDIOPORT_TYPE_HEADPHONES
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry represnting a headphone port
 */
#define IOAUDIOPORT_TYPE_HEADPHONES	"Headphones"

/*!
 * @defined IOAUDIOPORT_TYPE_MICROPHONE
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a microphone port
 */
#define IOAUDIOPORT_TYPE_MICROPHONE	"Microphone"

/*!
 * @defined IOAUDIOPORT_TYPE_CD
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a cd port
 */
#define IOAUDIOPORT_TYPE_CD		"CD"

/*!
 * @defined IOAUDIOPORT_TYPE_LINE
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a line level port
 */
#define IOAUDIOPORT_TYPE_LINE		"Line"

/*!
 * @defined IOAUDIOPORT_TYPE_DIGITAL
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a digital audio port
 */
#define IOAUDIOPORT_TYPE_DIGITAL	"Digital"

/*!
 * @defined IOAUDIOPORT_TYPE_MIXER
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a mixer port
 * @discussion Typically this represents an internal mixer unit.
 */
#define IOAUDIOPORT_TYPE_MIXER		"Mixer"

/*!
 * @defined IOAUDIOPORT_TYPE_PASSTHRU
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a pass through port
 * @discussion Typically this is an internal entity on the signal chain which is used to control passing
 *  the audio input to the output.
 */
#define IOAUDIOPORT_TYPE_PASSTHRU	"PassThru"

/*!
 * @defined IOAUDIOPORT_TYPE_MODEM
 * @abstract The value for the IOAUDIOPORT_TYPE_KEY in the IORegistry representing a modem port
 * @discussion This typically represents an internal modem
 */
#define IOAUDIOPORT_TYPE_MODEM		"Modem"

/*!
 * @defined IOAUDIOPORT_SUBTYPE_KEY
 * @abstract The key in the IORegistry for the IOAudioPort subtype attribute.
 * @discussion The IOAudioPort subtype is a driver-defined text attribute designed to complement the type
 *  attribute.
 */
#define IOAUDIOPORT_SUBTYPE_KEY		"IOAudioPortSubtype"

/*!
 * @defined IOAUDIOPORT_NAME_KEY
 * @abstract The key in the IORegistry for the IOAudioPort name attribute.
 */
#define IOAUDIOPORT_NAME_KEY		"IOAudioPortName"



/*****
 *
 * IOAudioControl defines
 *
 *****/
 
 
 /*!
 * @defined IOAUDIOCONTROL_TYPE_KEY
 * @abstract The key in the IORegistry for the IOAudioCntrol type attribute.
 * @discussion The value of this text attribute may be defined by the driver, however system-defined
 *  types recognized by the upper-level software are "Level", "Mute" and "Jack".
 */
#define IOAUDIOCONTROL_TYPE_KEY		"IOAudioControlType"

/*!
 * @defined IOAUDIOCONTROL_TYPE_LEVEL
 * @abstract The value for the IOAUDIOCONTROL_TYPE_KEY in the IORegistry representing a Level IOAudioControl
 */
#define IOAUDIOCONTROL_TYPE_LEVEL	"Level"

/*!
 * @defined IOAUDIOCONTROL_TYPE_MUTE
 * @abstract The value for the IOAUDIOCONTROL_TYPE_KEY in the IORegistry representing a Mute IOAudioControl
 */
#define IOAUDIOCONTROL_TYPE_MUTE	"Mute"

/*!
 * @defined IOAUDIOCONTROL_TYPE_JACK
 * @abstract The value for the IOAUDIOCONTROL_TYPE_KEY in the IORegistry representing a Jack IOAudioControl
 */
#define IOAUDIOCONTROL_TYPE_JACK	"Jack"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_ID_KEY
 * @abstract The key in the IORegistry for the IOAudioControl channel ID attribute
 * @discussion The value for this key is an integer which may be driver defined.  Default values for
 *  common channel types are provided in the following defines.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_KEY			"IOAudioControlChannelID"

/*! 
 * @defined IOAUDIOCONTROL_CHANNEL_ID_ALL
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_ID_KEY in the IORegistry representing
 *  the channel ID for all channels.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_ALL			0

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_LEFT
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_ID_KEY in the IORegistry representing
 *  the default channel ID for the left channel.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_LEFT		1

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_RIGHT
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_ID_KEY in the IORegistry representing
 *  the default channel ID for the right channel.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_RIGHT		2

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_CENTER
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_ID_KEY in the IORegistry representing
 *  the default channel ID for the center channel.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_CENTER	3

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_LEFT_REAR
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_ID_KEY in the IORegistry representing
 *  the default channel ID for the left rear channel.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_LEFT_REAR	4

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_RIGHT_REAR
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_ID_KEY in the IORegistry representing
 *  the default channel ID for the right rear channel.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_RIGHT_REAR	5

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_SUB
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_ID_KEY in the IORegistry representing
 *  the default channel for the sub/LFE channel.
 */
#define IOAUDIOCONTROL_CHANNEL_ID_DEFAULT_SUB		6


/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_KEY
 * @abstract The key in the IORegistry for the IOAudioControl name attribute.
 * @discussion This name should be a human-readable name for the channel(s) represented by the port.
 *  *** NOTE *** We really need to make all of the human-readable attributes that have potential to
 *  be used in a GUI localizable.  There will need to be localized strings in the kext bundle matching
 *  the text.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_KEY		"IOAudioControlChannelName"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_ALL
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_NAME_KEY in the IORegistry representing
 *  the channel name for all channels.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_ALL		"All Channels"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_LEFT
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_NAME_KEY in the IORegistry representing
 *  the channel name for the left channel.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_LEFT	"Left"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_RIGHT
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_NAME_KEY in the IORegistry representing
 *  the channel name for the right channel.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_RIGHT	"Right"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_CENTER
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_NAME_KEY in the IORegistry representing
 *  the channel name for the center channel.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_CENTER	"Center"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_LEFT_REAR
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_NAME_KEY in the IORegistry representing
 *  the channel name for the left rear channel.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_LEFT_REAR	"LeftRear"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_RIGHT_REAR
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_NAME_KEY in the IORegistry representing
 *  the channel name for the right rear channel.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_RIGHT_REAR	"RightRear"

/*!
 * @defined IOAUDIOCONTROL_CHANNEL_NAME_SUB
 * @abstract The value for the IOAUDIOCONTROL_CHANNEL_NAME_KEY in the IORegistry representing
 *  the channel name for the sub/LFE channel.
 */
#define IOAUDIOCONTROL_CHANNEL_NAME_SUB		"Sub"

/*!
 * @defined IOAUDIOCONTROL_VALUE_KEY
 * @abstract The key in the IORegistry for the IOAudioControl value attribute.
 * @discussion The value returned by this key is a 32-bit integer representing the current value of the IOAudioControl.
 */
#define IOAUDIOCONTROL_VALUE_KEY	"IOAudioControlValue"

/*!
 * @defined IOAUDIOCONTROL_MIN_VALUE_KEY
 * @abstract The key in the IORegistry for the IOAudioControl minimum value attribute.
 * @discussion The value returned by this key is a 32-bit integer representing the minimum value for the IOAudioControl.
 *  This is currently only valid for Level controls or other driver-defined controls that have a minimum and maximum
 *  value.
 */
#define IOAUDIOCONTROL_MIN_VALUE_KEY	"IOAudioControlMinValue"

/*!
 * @defined IOAUDIOCONTROL_MAX_VALUE_KEY
 * @abstract The key in the IORegistry for the IOAudioControl maximum value attribute.
 * @discussion The value returned by this key is a 32-bit integer representing the maximum value for the IOAudioControl.
 *  This is currently only valid for Level controls or other driver-defined controls that have a minimum and maximum
 *  value.
 */
#define IOAUDIOCONTROL_MAX_VALUE_KEY	"IOAudioControlMaxValue"

/*!
 * @defined IOAUDIOCONTROL_MIN_DB_KEY
 * @abstract The key in the IORgistry for the IOAudioControl minimum db value attribute.
 * @discussion The value returned by this key is a fixed point value in 16.16 format represented as a 32-bit
 *  integer.  It represents the minimum value in db for the IOAudioControl.  This value matches the minimum
 *  value attribute.  This is currently valid for Level controls or other driver-defined controls that have a
 *  minimum and maximum db value.
 */
#define IOAUDIOCONTROL_MIN_DB_KEY	"IOAudioControlMinDB"

/*!
 * @defined IOAUDIOCONTROL_MAX_DB_KEY
 * @abstract The key in the IORgistry for the IOAudioControl maximum db value attribute.
 * @discussion The value returned by this key is a fixed point value in 16.16 format represented as a 32-bit
 *  integer.  It represents the maximum value in db for the IOAudioControl.  This value matches the maximum
 *  value attribute.  This is currently valid for Level controls or other driver-defined controls that have a
 *  minimum and maximum db value.
 */
#define IOAUDIOCONTROL_MAX_DB_KEY	"IOAudioControlMaxDB"


#endif /* _IOAUDIODEFINES_H */