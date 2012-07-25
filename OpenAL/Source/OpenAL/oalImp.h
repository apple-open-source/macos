/**********************************************************************************************************************************
*
*   OpenAL cross platform audio library
*   Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without modification, are permitted provided 
*   that the following conditions are met:
*
*   1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
*   2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
*       disclaimer in the documentation and/or other materials provided with the distribution. 
*   3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its contributors may be used to endorse or promote 
*       products derived from this software without specific prior written permission. 
*
*   THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
*   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS 
*   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
*   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
*   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
*   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
**********************************************************************************************************************************/

#ifndef __OAL_IMP__
#define __OAL_IMP__

#include "al.h"
#include "alc.h"
#include "oalOSX.h"
#include <Carbon/Carbon.h>
#include <map>

typedef ALvoid			(*alSourceNotificationProc) (ALuint sid, ALuint	notificationID, ALvoid*	userData);

#ifdef __cplusplus
extern "C" {
#endif

// added for OSX Extension 
ALC_API ALvoid alcMacOSXRenderingQuality (ALint value);
ALC_API ALvoid alMacOSXRenderChannelCount (ALint value);
ALC_API ALvoid alcMacOSXMixerMaxiumumBusses (ALint value);
ALC_API ALvoid alcMacOSXMixerOutputRate(ALdouble value);

ALC_API ALint alcMacOSXGetRenderingQuality ();
ALC_API ALint alMacOSXGetRenderChannelCount ();
ALC_API ALint alcMacOSXGetMixerMaxiumumBusses ();
ALC_API ALdouble alcMacOSXGetMixerOutputRate();

AL_API ALvoid AL_APIENTRY alSetInteger (ALenum pname, ALint value);
AL_API ALvoid AL_APIENTRY alSetDouble (ALenum pname, ALdouble value);

AL_API ALvoid	AL_APIENTRY	alBufferDataStatic (ALint bid, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq);

AL_API ALenum alSourceAddNotification (ALuint sid, ALuint notificationID, alSourceNotificationProc notifyProc, ALvoid* userData);
AL_API ALvoid alSourceRemoveNotification (ALuint	sid, ALuint notificationID, alSourceNotificationProc notifyProc, ALvoid* userData);

// added for ASA (Apple Environmental Audio) 

ALC_API ALenum  alcASAGetSource(ALuint property, ALuint source, ALvoid *data, ALuint* dataSize);
ALC_API ALenum  alcASASetSource(ALuint property, ALuint source, ALvoid *data, ALuint dataSize);
ALC_API ALenum  alcASAGetListener(ALuint property, ALvoid *data, ALuint* dataSize);
ALC_API ALenum  alcASASetListener(ALuint property, ALvoid *data, ALuint dataSize);

// Used internally but no longer available via a header file. Some OpenAL applications may have been built with a header
// that defined these constants so keep defining them.

#define ALC_SPATIAL_RENDERING_QUALITY        0xF002
#define ALC_MIXER_OUTPUT_RATE		         0xF003
#define ALC_MIXER_MAXIMUM_BUSSES             0xF004
#define ALC_RENDER_CHANNEL_COUNT             0xF005

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define AL_FORMAT_MONO_FLOAT32               0x10010
#define AL_FORMAT_STEREO_FLOAT32             0x10011

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Continue exporting these deprectaed APIs to prevent runtime link errors

#ifdef TARGET_OS_MAC
   #if TARGET_OS_MAC
       #pragma export on
   #endif
#endif


AL_API ALvoid	AL_APIENTRY alHint( ALenum target, ALenum mode );

#ifdef TARGET_OS_MAC
   #if TARGET_OS_MAC
      #pragma export off
   #endif
#endif

#ifdef __cplusplus
}
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// development build flags
#define	USE_AU_TRACER 					0
#define LOG_GRAPH_AND_MIXER_CHANGES		0
#define GET_OVERLOAD_NOTIFICATIONS 		0
#define	LOG_IO 							0

#if LOG_IO
	#include  "AudioLogger.h"
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define AL_MAXBUFFERS 1024	
#define AL_MAXSOURCES 256 

#define kDefaultMaximumMixerBusCount    64
#define kDopplerDefault                 0	

enum {
		kRogerBeepType	= 'rogr',
		kDistortionType	= 'dist'
};

#define	THROW_RESULT		if(result != noErr) throw static_cast<OSStatus>(result);

enum {
		kUnknown3DMixerVersion	= 0,
		kUnsupported3DMixer		= 1,
		k3DMixerVersion_1_3		= 13,
		k3DMixerVersion_2_0,
		k3DMixerVersion_2_1,
		k3DMixerVersion_2_2,
		k3DMixerVersion_2_3
};

enum {
		kUnknownAUState	= -1,
		kAUIsNotPresent	= 0,
		kAUIsPresent	= 1
};		
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void	alSetError (ALenum errorCode);
UInt32	Get3DMixerVersion ();
ALCint  IsDistortionPresent();
ALCint  IsRogerBeepPresent();

#endif

