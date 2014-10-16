/**********************************************************************************************************************************
* OpenAL cross platform audio library
* Copyright (C) 1999-2000 by authors.
* Portions Copyright (C) 2004 by Apple Computer Inc., Copyright (C) 2012 by Apple Inc.
* This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Library General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
*  License along with this library; if not, write to the
*  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
*  Boston, MA  02111-1307, USA.
* Or go to http://www.gnu.org/copyleft/lgpl.html
**********************************************************************************************************************************/

#include "oalOSX.h"

#define		LOG_RING_BUFFER		0

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** UTILITY *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Use this for getting a unique token when creating  new contexts and devices
uintptr_t	GetNewPtrToken (void)
{
	static	uintptr_t	currentToken = 24;
	uintptr_t	returnedToken;
	
	returnedToken = currentToken;
	currentToken++;
	
	return (returnedToken);
}

// Use this for getting a unique integer token
ALuint	GetNewToken (void)
{
	static	ALuint	currentToken = 2400;
	ALuint	returnedToken;
	
	returnedToken = currentToken;
	currentToken++;
	
	return (returnedToken);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const char		kPathDelimiter[] = "/";
const UInt32	kPathDelimiterLength = 1;

bool	IsRelativePath(const char* inPath)
{
	return inPath[0] != kPathDelimiter[0];
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	MakeAbsolutePath(const char* inRelativePath, char* outAbsolutePath, UInt32 inMaxAbsolutePathLength)
{
	//	get the path to the current working directory
	getcwd(outAbsolutePath, inMaxAbsolutePathLength);
	if ((strlen(outAbsolutePath) + kPathDelimiterLength) <= inMaxAbsolutePathLength - 1)
	{
		strcat(outAbsolutePath, kPathDelimiter);
		if ((strlen(outAbsolutePath) + strlen(inRelativePath)) <= inMaxAbsolutePathLength - 1)
		{
			strcat(outAbsolutePath, inRelativePath);
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	GetOALFormatFromASBD(CAStreamBasicDescription	&inASBD)
{
	switch (inASBD.mFormatID)
	{
		case kAudioFormatLinearPCM:
			// NOTE: if float: check for extension
			
			if (inASBD.mFormatFlags & kAudioFormatFlagIsFloat)  
			{
				if (inASBD.NumberChannels() == 1 && inASBD.mBitsPerChannel == 32)
				{
					if(alIsExtensionPresent("AL_EXT_float32"))
						return alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
				}
				if (inASBD.NumberChannels() == 2 && inASBD.mBitsPerChannel == 32)
				{
					if(alIsExtensionPresent("AL_EXT_float32"))
						return alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");					
				}				
			}
			else
			{
				if (inASBD.NumberChannels() == 1 && inASBD.mBitsPerChannel == 16)
					return AL_FORMAT_MONO16;
				else if (inASBD.NumberChannels() == 2 && inASBD.mBitsPerChannel == 16)
					return AL_FORMAT_STEREO16;
				else if (inASBD.NumberChannels() == 1 && inASBD.mBitsPerChannel == 8)
					return AL_FORMAT_MONO8;
				else if (inASBD.NumberChannels() == 2 && inASBD.mBitsPerChannel == 8)
					return AL_FORMAT_STEREO8;
			}
			break;
		default:
			return (0);
			break;
	}
	return (0);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const char* GetFormatString(UInt32 inToken)
{
	switch(inToken)
	{
		case AL_FORMAT_MONO16:
			return "16-Bit/Mono";
			break;
		case AL_FORMAT_STEREO16:
			return "16-Bit/Stereo";
			break;
		case AL_FORMAT_MONO8:
			return "8-Bit/Mono";
			break;
		case AL_FORMAT_STEREO8:
			return "8-Bit/Stereo";
			break;
	}
	return "UNKNOWN FORMAT";
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// For use with DebugMessages so errors are more readable
const char* GetALAttributeString(UInt32 inToken)
{
	switch(inToken)
	{
		case AL_SOURCE_RELATIVE: return "AL_SOURCE_RELATIVE"; break;
		case AL_CONE_INNER_ANGLE: return "AL_CONE_INNER_ANGLE"; break;
		case AL_CONE_OUTER_ANGLE: return "AL_CONE_OUTER_ANGLE"; break;
		case AL_CONE_OUTER_GAIN: return "AL_CONE_OUTER_GAIN"; break;
		case AL_PITCH: return "AL_PITCH"; break;
		case AL_POSITION: return "AL_POSITION"; break;
		case AL_DIRECTION: return "AL_DIRECTION"; break;
		case AL_VELOCITY: return "AL_VELOCITY"; break;
		case AL_LOOPING: return "AL_LOOPING"; break;
		case AL_BUFFER: return "AL_BUFFER"; break;
		case AL_GAIN: return "AL_GAIN"; break;
		case AL_MIN_GAIN: return "AL_MIN_GAIN"; break;
		case AL_MAX_GAIN: return "AL_MAX_GAIN"; break;
		case AL_ORIENTATION: return "AL_ORIENTATION"; break;
		case AL_REFERENCE_DISTANCE: return "AL_REFERENCE_DISTANCE"; break;
		case AL_ROLLOFF_FACTOR: return "AL_ROLLOFF_FACTOR"; break;
		case AL_MAX_DISTANCE: return "AL_MAX_DISTANCE"; break;
		case AL_SOURCE_STATE: return "AL_SOURCE_STATE"; break;
		case AL_BUFFERS_QUEUED: return "AL_BUFFERS_QUEUED"; break;
		case AL_BUFFERS_PROCESSED: return "AL_BUFFERS_PROCESSED"; break;
		case AL_SEC_OFFSET: return "AL_SEC_OFFSET"; break;
		case AL_SAMPLE_OFFSET: return "AL_SAMPLE_OFFSET"; break;
		case AL_BYTE_OFFSET: return "AL_BYTE_OFFSET"; break;
		case AL_SOURCE_TYPE: return "AL_SOURCE_TYPE"; break;
		case AL_NONE: return "AL_NONE"; break;
		case AL_INVERSE_DISTANCE: return "AL_INVERSE_DISTANCE"; break;
		case AL_INVERSE_DISTANCE_CLAMPED: return "AL_INVERSE_DISTANCE_CLAMPED"; break;
		case AL_LINEAR_DISTANCE: return "AL_LINEAR_DISTANCE"; break;
		case AL_LINEAR_DISTANCE_CLAMPED: return "AL_LINEAR_DISTANCE_CLAMPED"; break;
		case AL_EXPONENT_DISTANCE: return "AL_EXPONENT_DISTANCE"; break;
		case AL_EXPONENT_DISTANCE_CLAMPED: return "AL_EXPONENT_DISTANCE_CLAMPED"; break;
		case AL_INVALID_NAME: return "AL_INVALID_NAME"; break;
	}
	return "UNKNOWN ATTRIBUTE - WARNING WARNING WARNING";
}

const char* GetALCAttributeString(UInt32 inToken)
{
	switch(inToken)
	{
		case ALC_DEFAULT_DEVICE_SPECIFIER: return "ALC_DEFAULT_DEVICE_SPECIFIER"; break;
		case ALC_DEVICE_SPECIFIER: return "ALC_DEVICE_SPECIFIER"; break;
		case ALC_EXTENSIONS: return "ALC_EXTENSIONS"; break;
		case ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER: return "ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER"; break;
		case ALC_CAPTURE_DEVICE_SPECIFIER: return "ALC_CAPTURE_DEVICE_SPECIFIER"; break;
		case ALC_NO_ERROR: return "ALC_NO_ERROR"; break;
		case ALC_INVALID_DEVICE: return "ALC_INVALID_DEVICE"; break;
		case ALC_INVALID_CONTEXT: return "ALC_INVALID_CONTEXT"; break;
		case ALC_INVALID_ENUM: return "ALC_INVALID_ENUM"; break;
		case ALC_INVALID_VALUE: return "ALC_INVALID_VALUE"; break;

		case ALC_ASA_REVERB_ON: return "ALC_ASA_REVERB_ON"; break;
		case ALC_ASA_REVERB_ROOM_TYPE: return "ALC_ASA_REVERB_ROOM_TYPE"; break;
		case ALC_ASA_REVERB_SEND_LEVEL: return "ALC_ASA_REVERB_SEND_LEVEL"; break;
		case ALC_ASA_REVERB_GLOBAL_LEVEL: return "ALC_ASA_REVERB_GLOBAL_LEVEL"; break;
		case ALC_ASA_OCCLUSION: return "ALC_ASA_OCCLUSION"; break;
		case ALC_ASA_ROGER_BEEP_ENABLE: return "ALC_ASA_ROGER_BEEP_ENABLE"; break;
		case ALC_ASA_ROGER_BEEP_ON: return "ALC_ASA_ROGER_BEEP_ON"; break;
		case ALC_ASA_ROGER_BEEP_GAIN: return "ALC_ASA_ROGER_BEEP_GAIN"; break;
		case ALC_ASA_ROGER_BEEP_SENSITIVITY: return "ALC_ASA_ROGER_BEEP_SENSITIVITY"; break;
		case ALC_ASA_ROGER_BEEP_TYPE: return "ALC_ASA_ROGER_BEEP_TYPE"; break;
		case ALC_ASA_ROGER_BEEP_PRESET: return "ALC_ASA_ROGER_BEEP_PRESET"; break;

		case ALC_ASA_DISTORTION_ENABLE: return "ALC_ASA_DISTORTION_ENABLE"; break;
		case ALC_ASA_DISTORTION_ON: return "ALC_ASA_DISTORTION_ON"; break;
		case ALC_ASA_DISTORTION_MIX: return "ALC_ASA_DISTORTION_MIX"; break;
		case ALC_ASA_DISTORTION_TYPE: return "ALC_ASA_DISTORTION_TYPE"; break;
		case ALC_ASA_DISTORTION_PRESET: return "ALC_ASA_DISTORTION_PRESET"; break;
	}
	return "UNKNOWN ATTRIBUTE - WARNING WARNING WARNING";
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool IsValidRenderQuality (UInt32 inRenderQuality)
{	
	switch (inRenderQuality)
	{
		case ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_HIGH:
		case ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_LOW:
			return (true);
			break;
			
		default:
			return (false);
			break;
	}

	return (false);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	IsFormatSupported(UInt32	inFormatID)
{
	switch(inFormatID)
	{
		case AL_FORMAT_MONO16:
		case AL_FORMAT_STEREO16:
		case AL_FORMAT_MONO8:
		case AL_FORMAT_STEREO8:
			return true;
			break;
		case AL_FORMAT_MONO_FLOAT32:
		case AL_FORMAT_STEREO_FLOAT32:
			return alIsExtensionPresent("AL_EXT_float32");
			break;
		default:
			return false;
			break;
	}
	return false;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	FillInASBD(CAStreamBasicDescription &inASBD, UInt32	inFormatID, UInt32 inSampleRate)
{
	OSStatus	err = noErr;
	
	switch (inFormatID)
	{
		case AL_FORMAT_STEREO16:
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 4;
			inASBD.mBytesPerFrame = 4;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 16;
			inASBD.mChannelsPerFrame = 2;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_MONO16: 
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 2;
			inASBD.mBytesPerFrame = 2;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 16;
			inASBD.mChannelsPerFrame = 1;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_STEREO8:
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 2;
			inASBD.mBytesPerFrame = 2;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 8;
			inASBD.mChannelsPerFrame = 2;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_MONO8 : 
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 1;
			inASBD.mBytesPerFrame = 1;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 8;
			inASBD.mChannelsPerFrame = 1;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_STEREO_FLOAT32 : 
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kLinearPCMFormatFlagIsFloat;
			inASBD.mBytesPerPacket = 8;
			inASBD.mBytesPerFrame = 8;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 32;
			inASBD.mChannelsPerFrame = 2;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_MONO_FLOAT32 : 
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kLinearPCMFormatFlagIsFloat;
			inASBD.mBytesPerPacket = 4;
			inASBD.mBytesPerFrame = 4;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 32;
			inASBD.mChannelsPerFrame = 1;
			inASBD.mReserved = 0;
			break;
		default: 
			err = AL_INVALID_VALUE;
			break;
	}
	return (err);	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	GetDefaultDeviceName(ALCchar*		outDeviceName, bool	isInput)
{
	UInt32		size = 0;
	OSStatus	result = noErr;
	UInt32		deviceProperty = isInput ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice;
	
	try {
		AudioDeviceID	defaultDevice = 0;
		// Get the default output device
		size = sizeof(defaultDevice);
		result = AudioHardwareGetProperty(deviceProperty, &size, &defaultDevice);
			THROW_RESULT

		result = AudioDeviceGetPropertyInfo( defaultDevice, 0, false, kAudioDevicePropertyDeviceName, &size, NULL);
			THROW_RESULT

		if (size > maxLen)
			throw -1;
		
		size = maxLen;
		result = AudioDeviceGetProperty(defaultDevice, 0, false, kAudioDevicePropertyDeviceName, &size, outDeviceName);
			THROW_RESULT
			
	} catch (...) {
		outDeviceName[0] = '\0'; // failure case, make it a zero length string
	}
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	CalculateNeededMixerBusses(const ALCint *attrList, UInt32 inDefaultBusCount)
{
	UInt32		monoSources = 0;
	UInt32		stereoSources = 0;
	UInt32		returnValue = inDefaultBusCount; // default case
	ALCint*		currentAttribute = ( ALCint*) attrList;

	// ATTRIBUTE LIST
	if (attrList)
	{
		while (*currentAttribute != 0)
		{
			switch (*currentAttribute)
			{
				case ALC_MONO_SOURCES:
					monoSources = currentAttribute[1];
					break;
				case ALC_STEREO_SOURCES:
					stereoSources = currentAttribute[1];
					break;
				default:
					break;
			}
			currentAttribute += 2;
		}

		if (monoSources == 0 && stereoSources == 0)
			returnValue = inDefaultBusCount;
		else if ((monoSources + stereoSources > kDefaultMaximumMixerBusCount) || (monoSources + stereoSources > inDefaultBusCount))
			returnValue = monoSources + stereoSources;
	}
			
	return returnValue;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	GetDesiredRenderChannelsFor3DMixer(UInt32	inDeviceChannels)
{
    UInt32	returnValue = inDeviceChannels;
	
	if ((Get3DMixerVersion() < k3DMixerVersion_2_0) && (returnValue == 4))
    {
        // quad did not work properly before version 2.0 of the 3DMixer, so just render to stereo
        returnValue = 2;
    }
    else if (inDeviceChannels < 4)
    {
        // guard against the possibility of multi channel hw that has never been given a preferred channel layout
        // Or, that a 3 channel layout was returned (which is unsupported by the 3DMixer)
        returnValue = 2; 
    }
    else if ((inDeviceChannels > 5) &&  (Get3DMixerVersion() < k3DMixerVersion_2_3))
    {
		// 3DMixer ver. 2.2 and below does not render to more than 5 channels
		returnValue = 5;
	}
	else if (inDeviceChannels > 8)
    {
		// Current 3DMixer renders to maximum 8 channels
		returnValue = 8;
	}
	
	return returnValue;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** DEPRECATED ALUT *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifdef __cplusplus
extern "C" {
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// alutLoadWAVMemory()
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define ALUTAPI
#define ALUTAPIENTRY

ALUTAPI ALvoid	ALUTAPIENTRY alutInit(ALint *argc,ALbyte **argv);
ALUTAPI ALvoid	ALUTAPIENTRY alutExit(void);
ALUTAPI ALvoid	ALUTAPIENTRY alutLoadWAVFile(ALbyte *file,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq);
ALUTAPI ALvoid  ALUTAPIENTRY alutLoadWAVMemory(ALbyte *memory,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq);
ALUTAPI ALvoid  ALUTAPIENTRY alutUnloadWAV(ALenum format,ALvoid *data,ALsizei size,ALsizei freq);

/*
    alutLoadWAVMemory() existed in previous OAL implementations, and is provided for legacy purposes.
    This is the same implementation already existing in the Open Source repository.
*/

typedef struct                                  /* WAV File-header */
{
  ALubyte  Id[4];
  ALsizei  Size;
  ALubyte  Type[4];
} WAVFileHdr_Struct;

typedef struct                                  /* WAV Fmt-header */
{
  ALushort Format;                              
  ALushort Channels;
  ALuint   SamplesPerSec;
  ALuint   BytesPerSec;
  ALushort BlockAlign;
  ALushort BitsPerSample;
} WAVFmtHdr_Struct;

typedef struct									/* WAV FmtEx-header */
{
  ALushort Size;
  ALushort SamplesPerBlock;
} WAVFmtExHdr_Struct;

typedef struct                                  /* WAV Smpl-header */
{
  ALuint   Manufacturer;
  ALuint   Product;
  ALuint   SamplePeriod;                          
  ALuint   Note;                                  
  ALuint   FineTune;                              
  ALuint   SMPTEFormat;
  ALuint   SMPTEOffest;
  ALuint   Loops;
  ALuint   SamplerData;
  struct
  {
    ALuint Identifier;
    ALuint Type;
    ALuint Start;
    ALuint End;
    ALuint Fraction;
    ALuint Count;
  }      Loop[1];
} WAVSmplHdr_Struct;

typedef struct                                  /* WAV Chunk-header */
{
  ALubyte  Id[4];
  ALuint   Size;
} WAVChunkHdr_Struct;

void SwapWords(unsigned int *puint);
void SwapBytes(unsigned short *pshort);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SwapWords(unsigned int *puint)
{
    unsigned int tempint;
	char *pChar1, *pChar2;
	
	tempint = *puint;
	pChar2 = (char *)&tempint;
	pChar1 = (char *)puint;
	
	pChar1[0]=pChar2[3];
	pChar1[1]=pChar2[2];
	pChar1[2]=pChar2[1];
	pChar1[3]=pChar2[0];
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SwapBytes(unsigned short *pshort)
{
    unsigned short tempshort;
    char *pChar1, *pChar2;
    
    tempshort = *pshort;
    pChar2 = (char *)&tempshort;
    pChar1 = (char *)pshort;
    
    pChar1[0]=pChar2[1];
    pChar1[1]=pChar2[0];
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutLoadWAVMemory(ALbyte *memory,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq)
{
	WAVChunkHdr_Struct ChunkHdr;
	WAVFmtExHdr_Struct FmtExHdr;
	WAVFileHdr_Struct FileHdr;
	WAVSmplHdr_Struct SmplHdr;
	WAVFmtHdr_Struct FmtHdr;
	ALbyte *Stream;
	
	*format=AL_FORMAT_MONO16;
	*data=NULL;
	*size=0;
	*freq=22050;
	if (memory)
	{		
		Stream=memory;
		if (Stream)
		{
		    memcpy(&FileHdr,Stream,sizeof(WAVFileHdr_Struct));
		    Stream+=sizeof(WAVFileHdr_Struct);
			SwapWords((unsigned int*) &FileHdr.Size);
			FileHdr.Size=((FileHdr.Size+1)&~1)-4;
			while ((FileHdr.Size!=0)&&(memcpy(&ChunkHdr,Stream,sizeof(WAVChunkHdr_Struct))))
			{
				Stream+=sizeof(WAVChunkHdr_Struct);
			    SwapWords(&ChunkHdr.Size);
			    
				if ((ChunkHdr.Id[0] == 'f') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == ' '))
				{
					memcpy(&FmtHdr,Stream,sizeof(WAVFmtHdr_Struct));
				    SwapBytes(&FmtHdr.Format);
					if (FmtHdr.Format==0x0001)
					{
					    SwapBytes(&FmtHdr.Channels);
					    SwapBytes(&FmtHdr.BitsPerSample);
					    SwapWords(&FmtHdr.SamplesPerSec);
					    SwapBytes(&FmtHdr.BlockAlign);
					    
						*format=(FmtHdr.Channels==1?
								(FmtHdr.BitsPerSample==8?AL_FORMAT_MONO8:AL_FORMAT_MONO16):
								(FmtHdr.BitsPerSample==8?AL_FORMAT_STEREO8:AL_FORMAT_STEREO16));
						*freq=FmtHdr.SamplesPerSec;
						Stream+=ChunkHdr.Size;
					} 
					else
					{
						memcpy(&FmtExHdr,Stream,sizeof(WAVFmtExHdr_Struct));
						Stream+=ChunkHdr.Size;
					}
				}
				else if ((ChunkHdr.Id[0] == 'd') && (ChunkHdr.Id[1] == 'a') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == 'a'))
				{
					if (FmtHdr.Format==0x0001)
					{
						*size=ChunkHdr.Size;
                            if(*data == NULL){
							*data=malloc(ChunkHdr.Size + 31);
							memset(*data,0,ChunkHdr.Size+31);
						}
						else{
							realloc(*data,ChunkHdr.Size + 31);
							memset(*data,0,ChunkHdr.Size+31);
						}
						if (*data) 
						{
							memcpy(*data,Stream,ChunkHdr.Size);
						    memset(((char *)*data)+ChunkHdr.Size,0,31);
							Stream+=ChunkHdr.Size;
						    if (FmtHdr.BitsPerSample == 16) 
						    {
						        for (UInt32 i = 0; i < (ChunkHdr.Size / 2); i++)
						        {
						        	SwapBytes(&(*(unsigned short **)data)[i]);
						        }
						    }
						}
					}
					else if (FmtHdr.Format==0x0011)
					{
						//IMA ADPCM
					}
					else if (FmtHdr.Format==0x0055)
					{
						//MP3 WAVE
					}
				}
				else if ((ChunkHdr.Id[0] == 's') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 'p') && (ChunkHdr.Id[3] == 'l'))
				{
				   	memcpy(&SmplHdr,Stream,sizeof(WAVSmplHdr_Struct));
					Stream+=ChunkHdr.Size;
				}
				else Stream+=ChunkHdr.Size;
				Stream+=ChunkHdr.Size&1;
				FileHdr.Size-=(((ChunkHdr.Size+1)&~1)+8);
			}
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutInit(ALint *argc,ALbyte **argv) 
{
    ALCcontext *Context;
    ALCdevice *Device;
	
    Device=alcOpenDevice(NULL);  //Open device
 	
    if (Device != NULL) {
        Context=alcCreateContext(Device,0);  //Create context
		
	if (Context != NULL) {
	    alcMakeContextCurrent(Context);  //Set active context
	}
    }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutExit(void) 
{
    ALCcontext *Context;
    ALCdevice *Device;
	
    //Get active context
    Context=alcGetCurrentContext();
    //Get device for active context
    Device=alcGetContextsDevice(Context);
    //Release context
    alcDestroyContext(Context);
    //Close device
    alcCloseDevice(Device);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutLoadWAVFile(ALbyte *file,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq)
{
	OSStatus		err = noErr;
	AudioFileID		audioFile = 0;
	FSRef			fsRef;

	*data = NULL; // in case of failure, do not return some unitialized value as a bogus address

	if (IsRelativePath(file))
	{
		char			absolutePath[256];
		// we need to make a full path here so FSPathMakeRef() works properly
		MakeAbsolutePath(file, absolutePath, 256);
		// create an fsref from the file parameter
		err = FSPathMakeRef ((const UInt8 *) absolutePath, &fsRef, NULL);
	}
	else
		err = FSPathMakeRef ((const UInt8 *) file, &fsRef, NULL);
	
	if (err == noErr)
	{
		err = AudioFileOpen(&fsRef, fsRdPerm, 0, &audioFile);
		if (err == noErr)
		{
			UInt32							dataSize;
			CAStreamBasicDescription		asbd;
			
			dataSize = sizeof(CAStreamBasicDescription);
			AudioFileGetProperty(audioFile, kAudioFilePropertyDataFormat, &dataSize, &asbd);
			
			*format = GetOALFormatFromASBD(asbd);
			if (IsFormatSupported(*format))
			{
				*freq = (UInt32) asbd.mSampleRate;
				
				SInt64	audioDataSize = 0;
				dataSize = sizeof(audioDataSize);
				err = AudioFileGetProperty(audioFile, kAudioFilePropertyAudioDataByteCount, &dataSize, &audioDataSize);
				if (err == noErr)
				{
					*size = audioDataSize;
					*data = NULL;
					*data = calloc(1, audioDataSize);
					if (*data)
					{
						dataSize = audioDataSize;
						/*err =*/ AudioFileReadBytes(audioFile, false, 0, &dataSize, *data);
						
#if TARGET_RT_BIG_ENDIAN
						// Only swap to big endian if running on a ppc mac
						if ((asbd.mFormatID == kAudioFormatLinearPCM) && (asbd.mBitsPerChannel > 8))
						{
							// we just got 16 bit pcm data out of a WAVE file on a big endian platform, so endian swap the data
							AudioConverterRef				converter;
							CAStreamBasicDescription		outFormat = asbd;
							void *							tempData = NULL;
							
							// ste format to big endian
							outFormat.mFormatFlags = kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
							// make some place for converted data
							tempData = calloc(1 , audioDataSize);
							
							err = AudioConverterNew(&asbd, &outFormat, &converter);
							if ((err == noErr) && (tempData != NULL))
							{
								UInt32		bufferSize = audioDataSize;
								err = AudioConverterConvertBuffer(converter, audioDataSize, *data, &bufferSize, tempData);
								if (err == noErr)
									memcpy(*data, tempData, audioDataSize);
								AudioConverterDispose(converter);
							}
							if (tempData) free (tempData);
						}
#endif // TARGET_RT_BIG_ENDIAN
		
					}
				}
			}
			/*err =*/ AudioFileClose(audioFile);
		}
	}
    else
        alSetError(err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutUnloadWAV(ALenum format,ALvoid *data,ALsizei size,ALsizei freq)
{
	if (data)
		free(data);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AL_MAIN functions
AL_API ALvoid AL_APIENTRY alInit(ALint *argc, ALubyte **argv)
{
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alExit(void)
{
}

#ifdef __cplusplus
}
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALRingBuffer *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// count the leading zeroes in a word
static __inline__ int CountLeadingZeroes(int arg) {

#if TARGET_CPU_X86 || TARGET_CPU_X86_64
	__asm__ volatile(
				"bsrl %0, %0\n\t"
				"movl $63, %%ecx\n\t"
				"cmove %%ecx, %0\n\t"
				"xorl $31, %0" 
				: "=r" (arg) 
				: "0" (arg)
			);
#elif TARGET_CPU_PPC || TARGET_CPU_PPC64	
	__asm__ volatile("cntlzw %0, %1" : "=r" (arg) : "r" (arg));
#else
	#error "ERROR - assembly instructions for counting leading zeroes not present"
#endif
         return arg;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// base 2 log of next power of two greater or equal to x
inline UInt32 Log2Ceil(UInt32 x)
{
	return 32 - CountLeadingZeroes(x - 1);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// next power of two greater or equal to x
inline UInt32 NextPowerOfTwo(UInt32 x)
{
	return 1L << Log2Ceil(x);
}
