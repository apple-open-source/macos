/**********************************************************************************************************************************
*
*   OpenAL cross platform audio library
*	Copyright (c) 2004, Apple Computer, Inc., Copyright (c) 2012, Apple Inc. All rights reserved.
*
*	Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following 
*	conditions are met:
*
*	1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
*	2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
*		disclaimer in the documentation and/or other materials provided with the distribution. 
*	3.  Neither the name of Apple Inc. ("Apple") nor the names of its contributors may be used to endorse or promote products derived 
*		from this software without specific prior written permission. 
*
*	THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
*	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS 
*	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
*	AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
*	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
**********************************************************************************************************************************/
/*
	This source file contains all the entry points into the oal state via the defined OpenAL API set. 
*/

#include "oalImp.h"
#include "oalContext.h"
#include "oalDevice.h"
#include "oalSource.h"
#include "oalBuffer.h"
#include "oalCaptureDevice.h"
#include "MacOSX_OALExtensions.h"
	
// ~~~~~~~~~~~~~~~~~~~~~~
// development build flags
#define		LOG_API_USAGE		0
#define		LOG_BUFFER_USAGE	0
#define		LOG_SOURCE_USAGE	0
#define		LOG_CAPTURE_USAGE	0
#define		LOG_EXTRAS			0
#define		LOG_ERRORS			0
#define		LOG_ASA_USAGE		0

#define		kMajorVersion	1
#define		kMinorVersion	1


char*		alcExtensions = NULL;
char*		alExtensions = NULL;

//these will be used to construct the actual strings
#define		alcExtensionsBase		"ALC_EXT_CAPTURE ALC_ENUMERATION_EXT ALC_EXT_MAC_OSX"
#define		alcExtensionsASA		" ALC_EXT_ASA"
#define		alcExtensionsDistortion	" ALC_EXT_ASA_DISTORTION"
#define		alcExtensionsRogerBeep	" ALC_EXT_ASA_ROGER_BEEP"

#define		alExtensionsBase		"AL_EXT_OFFSET AL_EXT_LINEAR_DISTANCE AL_EXT_EXPONENT_DISTANCE AL_EXT_float32 AL_EXT_STATIC_BUFFER AL_EXT_SOURCE_NOTIFICATIONS AL_EXT_SOURCE_SPATIALIZATION"

// ~~~~~~~~~~~~~~~~~~~~~~
// VERSION
#define alVersion					"1.1"

#define 	unknownImplementationError "Unknown Internal Error"

// AL_STATE info
#define 	alVendor				"Apple Computer Inc."
#define		alRenderer				"Software"

#define		alNoError				"No Error"
#define		alErrInvalidName		"Invalid Name"
#define		alErrInvalidEnum		"Invalid Enum"
#define		alErrInvalidValue		"Invalid Value"
#define		alErrInvalidOp			"Invalid Operation"
#define		alErrOutOfMemory		"Out of Memory"

#define		alcErrInvalidDevice		"ALC Invalid Device"
#define		alcErrInvalidContext	"ALC Invalid Context"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// globals
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

UInt32		gCurrentError = 0;										// globally stored error code
uintptr_t	gCurrentContext = 0;                                    // token for the current context
uintptr_t	gCurrentDevice = 0;                                     // token for the device of the current context
UInt32      gMaximumMixerBusCount = kDefaultMaximumMixerBusCount;   // use gMaximumMixerBusCount for settinmg the bus count when a device is opened
ALdouble	gMixerOutputRate = 0.0;
ALint		gRenderChannelSetting = ALC_MAC_OSX_RENDER_CHANNEL_COUNT_MULTICHANNEL;	// default setting is multi channel
					
ALCchar 	gDefaultOutputDeviceName[maxLen];
ALCchar 	gDefaultOutputDeviceNameList[maxLen];
ALCchar		gDefaultInputDeviceName[maxLen];
ALCchar 	gDefaultInputDeviceNameList[maxLen];

// At this time, only mono CBR formats would work - no problem as only pcm formats are currently valid
// The feature is turned on using ALC_CONVERT_DATA_UPON_LOADING and the alEnable()/alDisable() APIs
bool        gConvertBufferNow = false;                              // do not convert data into mixer format by default

// global object maps
OALDeviceMap*			gOALDeviceMap			= NULL;			// this map will be created upon the first call to alcOpenDevice()
OALContextMap*			gOALContextMap			= NULL;			// this map will be created upon the first call to alcCreateContext()
OALBufferMap*			gOALBufferMap			= NULL;			// this map will be created upon the first call to alcGenBuffers()
OALBufferMap*			gDeadOALBufferMap		= NULL;			// this map will be created upon the first call to alcGenBuffers()
OALCaptureDeviceMap*	gOALCaptureDeviceMap	= NULL;			// this map will be created upon the first call to alcCaptureOpenDevice()

// global API lock
CAGuard*				gBufferMapLock			= NULL;			// this is used to prevent threading collisions in buffer manipulation API
CAGuard*				gContextMapLock			= NULL;			// this is used to prevent threading collisions in context manipulation API
CAGuard*				gDeviceMapLock			= NULL;			// this is used to prevent threading collisions in device manipulation API
CAGuard*				gCaptureDeviceMapLock	= NULL;			// this is used to prevent threading collisions in capture device manipulation API

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Support Methods *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void WaitOneRenderCycle()
{
	if (gOALContextMap == NULL) return;
	
	OALContext*		context = gOALContextMap->Get(gCurrentContext);
	if (context != NULL)
	{
		UInt32	microseconds = (UInt32)((context->GetFramesPerSlice() / (context->GetMixerRate()/1000)) * 1000);
		usleep (microseconds * 2);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
char* GetALCExtensionList()
{
	// if the string has already been allocated and created, return it
	if(alcExtensions != NULL) return alcExtensions;
	
	// first create the base extension string
	alcExtensions = (char*)malloc(strlen(alcExtensionsBase)+1);
	memcpy(alcExtensions, alcExtensionsBase, strlen(alcExtensionsBase)+1);
	
	// now add the extensions if they are found on the system
	if (Get3DMixerVersion() >= k3DMixerVersion_2_2)
	{
		alcExtensions = (char*)realloc(alcExtensions, strlen(alcExtensions) + strlen(alcExtensionsASA) + 1);
		strcat(alcExtensions, alcExtensionsASA);
	}
	
	if(IsDistortionPresent())
	{
		alcExtensions = (char*)realloc(alcExtensions, strlen(alcExtensions) + strlen(alcExtensionsDistortion) + 1);
		strcat(alcExtensions, alcExtensionsDistortion);
	}
	
	if(IsRogerBeepPresent())
	{
		alcExtensions = (char*)realloc(alcExtensions, strlen(alcExtensions) + strlen(alcExtensionsRogerBeep) + 1);
		strcat(alcExtensions, alcExtensionsRogerBeep);
	}
	
	return alcExtensions;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
char* GetALExtensionList()
{
	if(alExtensions != NULL) return alExtensions;
	
	// if list is not created, allocate and create a new string
	alExtensions = (char*)malloc(strlen(alExtensionsBase)+1);
	memcpy(alExtensions, alExtensionsBase, strlen(alExtensionsBase)+1);
	
	return alExtensions;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 Get3DMixerVersion ()
{
	static	UInt32		mixerVersion = kUnknown3DMixerVersion;
	
	if (mixerVersion == kUnknown3DMixerVersion)
	{
		ComponentDescription	mixerCD;
		mixerCD.componentFlags = 0;        
		mixerCD.componentFlagsMask = 0;     
		mixerCD.componentType = kAudioUnitType_Mixer;          
		mixerCD.componentSubType = kAudioUnitSubType_3DMixer;       
		mixerCD.componentManufacturer = kAudioUnitManufacturer_Apple;  

		ComponentInstance   mixerInstance = OpenComponent(FindNextComponent(0, &mixerCD));
		long  version = CallComponentVersion(mixerInstance);
		CloseComponent(mixerInstance);
		
		if (version < kMinimumMixerVersion)
		{
			mixerVersion = kUnsupported3DMixer;                           // we do not have a current enough 3DMixer to use
		}
		else if (version < 0x20000)
		{
			mixerVersion = k3DMixerVersion_1_3;
		}
		else if (version == 0x20000)
		{
			mixerVersion = k3DMixerVersion_2_0;
		}
		else if (version == 0x20100)
		{
			mixerVersion = k3DMixerVersion_2_1;
		}
		else if (version == 0x20200)
		{
			mixerVersion = k3DMixerVersion_2_2;
		}
		else if (version >= 0x20300)
		{
			mixerVersion = k3DMixerVersion_2_3;
		}
	}

    return	mixerVersion;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALdouble	GetMixerOutputRate()
{
	ALdouble	returnValue = 0.0;
	
	if (gMixerOutputRate > 0.0)
		returnValue = gMixerOutputRate; // return the user's explicit setting
	else 
	{
		OALDevice	*oalDevice = NULL;
		if (gOALDeviceMap)
			oalDevice = gOALDeviceMap->Get(gCurrentDevice);

		if (oalDevice)
		{
			// there has been no explicit setting yet, but there is an open device, return it's sample rate
			// as that will be the mixer setting as well, until gMixerOutputRate gets set before creating a context
			returnValue = (ALdouble) oalDevice->GetDeviceSampleRate();

		}
		else
		{
			// The default device has not yet been opened, go get the sample rate of the default hw
			AudioDeviceID	device;
			UInt32			propSize = sizeof(device);
			OSStatus	result = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propSize, &device);
			if (result == noErr)
			{
				Float64		sr;
				propSize = sizeof(sr);
				result = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyNominalSampleRate, &propSize, &sr);
				if (result == noErr)
					returnValue = sr;
			}
			else
			{
				throw (AL_INVALID_OPERATION);
			}
		}
	}
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void alSetError (ALenum errorCode)
{
    // only set an error if we are in a no error state
    if (gCurrentError == AL_NO_ERROR)
		gCurrentError = errorCode;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	CleanUpDeadBufferList()
{
	if (gDeadOALBufferMap)
	{
		UInt32	index = 0;
		UInt32	count = gDeadOALBufferMap->Size();
		for (UInt32 i = 0; i < count; i++)
		{
			OALBuffer*		buffer = gDeadOALBufferMap->GetBufferByIndex(index);
			if (buffer)
			{
				if (buffer->IsPurgable())
				{
					gDeadOALBufferMap->Remove(buffer->GetToken());
#if LOG_BUFFER_USAGE
	DebugMessageN1("CleanUpDeadBufferList--> deleting buffers = %ld", (long int) buffer->GetToken());
#endif
					delete (buffer);
				}
				else
					index++;
			}
			else
				index++;
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ReleaseContextObject must be called when finished using the returned object
OALContext*		ProtectContextObject (uintptr_t	inContextToken)
{
	OALContext		*oalContext = NULL;
	
	if (gOALContextMap == NULL)
		throw ((OSStatus) AL_INVALID_OPERATION);

	CAGuard::Locker locked(*gContextMapLock);
	
	oalContext = gOALContextMap->Get(inContextToken);
	if (oalContext == NULL)
		throw ((OSStatus) ALC_INVALID_CONTEXT);
	
	oalContext->SetInUseFlag();
		
	return oalContext;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReleaseContextObject(OALContext* inContext)
{
	if (inContext) inContext->ClearInUseFlag();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	SetDeviceError(uintptr_t inDeviceToken, UInt32	inError)
{
	if (gOALDeviceMap != NULL)
	{
		OALDevice			*oalDevice = gOALDeviceMap->Get(inDeviceToken);	// get the requested oal device
		if (oalDevice)
			oalDevice->SetError(inError);
	}
	
	if (gOALCaptureDeviceMap != NULL)
	{
		OALCaptureDevice			*oalCaptureDevice = gOALCaptureDeviceMap->Get(inDeviceToken);	// get the requested oal device
		if (oalCaptureDevice)
			oalCaptureDevice->SetError(inError);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ReleaseDeviceObject must be called when finished using the returned object
OALDevice*		ProtectDeviceObject (uintptr_t inDeviceToken)
{
	if (gOALDeviceMap == NULL)
		throw ((OSStatus)AL_INVALID_OPERATION);
	OALDevice *oalDevice = NULL;
	
	CAGuard::Locker locked(*gDeviceMapLock);

	oalDevice = gOALDeviceMap->Get(inDeviceToken);	// get the requested oal device
	if (oalDevice == NULL)
		throw ((OSStatus) AL_INVALID_VALUE);
	
	oalDevice->SetInUseFlag();
	
	return oalDevice;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	ReleaseDeviceObject(OALDevice* inDevice)
{
	if (inDevice) inDevice->ClearInUseFlag();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ReleaseCaptureDeviceObject must be called when finished using the returned object
OALCaptureDevice*		ProtectCaptureDeviceObject (uintptr_t inDeviceToken)
{
	if (gOALCaptureDeviceMap == NULL)
		throw ((OSStatus)AL_INVALID_OPERATION);

	OALCaptureDevice			*oalCaptureDevice = NULL;

	CAGuard::Locker locked(*gCaptureDeviceMapLock);

	oalCaptureDevice = gOALCaptureDeviceMap->Get(inDeviceToken);	// get the requested oal device
	if (oalCaptureDevice == NULL)
		throw ((OSStatus) AL_INVALID_VALUE);
	
	oalCaptureDevice->SetInUseFlag();
		
	return oalCaptureDevice;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	ReleaseCaptureDeviceObject(OALCaptureDevice* inDevice)
{
	if (inDevice) inDevice->ClearInUseFlag();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ReleaseSourceObject must be called when finished using the returned object
OALSource*	ProtectSourceObjectInCurrentContext(ALuint	inSID)
{
	OALContext	*oalContext = NULL;
	OALSource	*oalSource	= NULL;
	
	try {
		oalContext = ProtectContextObject(gCurrentContext);
		oalSource = oalContext->ProtectSource(inSID);
		if (oalSource == NULL)
			throw ((OSStatus) AL_INVALID_NAME);		// per OpenAL 1.1 spec

		ReleaseContextObject(oalContext);
	} 
	catch (OSStatus stat) {
		ReleaseContextObject(oalContext);
		throw stat;
	} 
	catch (...) {
		ReleaseContextObject(oalContext);
		throw -1;
	}
	
	return oalSource;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReleaseSourceObject(OALSource *inSource)
{
	if (inSource) inSource->ClearInUseFlag();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALBuffer*	ProtectBufferObject(ALuint	inBID)
{
	OALBuffer *oalBuffer = NULL;

	if (gOALBufferMap == NULL)
		throw ((OSStatus) AL_INVALID_OPERATION);

	CAGuard::Locker locked(*gBufferMapLock);
			
	CleanUpDeadBufferList();	

	oalBuffer = gOALBufferMap->Get(inBID);
	if (oalBuffer == NULL)
		throw ((OSStatus) AL_INVALID_VALUE);
	
	// we need to signal the buffer is being used, and cannot be deleted yet
	oalBuffer->SetInUseFlag();
	
	return oalBuffer;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ReleaseBufferObject must be called when finished using the returned object
void ReleaseBufferObject(OALBuffer* inBuffer)
{
	if (inBuffer) inBuffer->ClearInUseFlag();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALboolean	IsValidBufferObject(ALuint	inBID)
{
	if (inBID == 0) return true;	// 0 == AL_NONE which is valid
	
	ALboolean isBuffer = AL_FALSE;
	OALBuffer *oalBuffer = NULL;
	
	try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   
		
		oalBuffer = ProtectBufferObject(inBID);
		
        if (oalBuffer != NULL)
            isBuffer = AL_TRUE;
        else
            isBuffer = AL_FALSE;		
    }
	catch (OSStatus result) {
		isBuffer = AL_FALSE;		
		ReleaseBufferObject(oalBuffer);
		throw result;
	}
	ReleaseBufferObject(oalBuffer);
	return isBuffer;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	InitializeBufferMap()
{
	if (gOALBufferMap == NULL)
	{
		gOALBufferMap = new OALBufferMap ();						// create the buffer map since there isn't one yet
		gBufferMapLock = new CAGuard("OAL:BufferMapLock");				// create the buffer map mutex
		gDeadOALBufferMap = new OALBufferMap ();					// create the buffer map since there isn't one yet

		// populate the good buffer with the AL_NONE buffer, it should never be deleted
		OALBuffer	*newBuffer = new OALBuffer (AL_NONE);
		gOALBufferMap->Add(AL_NONE, &newBuffer);							// add the new buffer to the buffer map
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// walk through the context map and find the ones that are linked to this device
void	DeleteContextsOfThisDevice(uintptr_t inDeviceToken)
{
	if (gOALContextMap == NULL) 
		return;
		
	try {
		CAGuard::Locker locked(*gContextMapLock);
		
		for (UInt32	i = 0; i < gOALContextMap->Size(); i++)
		{
			uintptr_t		contextToken = 0;
			OALContext		*oalContext = gOALContextMap->GetContextByIndex(i, contextToken);
			
            if (oalContext == NULL)
                throw ((OSStatus) AL_INVALID_OPERATION);

			if (oalContext->GetDeviceToken() == inDeviceToken)
			{
				// delete this context, it belongs to the device that is going away
				if (contextToken == gCurrentContext)
				{
					// this context is the current context, so remove it as the current context first
					alcMakeContextCurrent(NULL);
				}
				
				if (gOALContextMap->Remove(contextToken)) {
					while(oalContext->IsInUse())
						usleep(10000);
						
					delete (oalContext);
					i--; // try this index again since it was just deleted
				}
			}
		}		
	}
	catch (OSStatus     result) {
		alSetError(result);
	}
	catch (...) {
		alSetError(AL_INVALID_VALUE);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Walk through the context map and reconfigure the ones that are linked to this device.
// They should all be disconnected right now so it is safe to do so - called from the device object
void	ReconfigureContextsOfThisDevice(uintptr_t inDeviceToken)
{
	if (gOALContextMap == NULL)
		return; // there aren't any contexts to configure

	uintptr_t		contextToken;
	OALContext		*oalContext = NULL;
	for (UInt32	i = 0; i < gOALContextMap->Size(); i++)
	{
		oalContext = gOALContextMap->GetContextByIndex(i, contextToken);
		if (oalContext != NULL && (oalContext->GetDeviceToken() == inDeviceToken))  {
			// found a context that uses this device
			oalContext->ConfigureMixerFormat();	// reconfigure the output format of the context's mixer
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCint   alcCheckUnitIsPresent(OSType componentSubType)
{
	ALCint isPresent = kUnknownAUState;
	
	ComponentDescription	desc;
	desc.componentFlags = 0;        
	desc.componentFlagsMask = 0;     
	desc.componentType = kAudioUnitType_Effect;          
	desc.componentSubType = componentSubType;       
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;  

	isPresent = (FindNextComponent(0, &desc) != 0) ? kAUIsPresent : kAUIsNotPresent;
	
	return isPresent;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCint  IsRogerBeepPresent()
{
	static	ALCint isPresent = kUnknownAUState;
	if (isPresent == kUnknownAUState)
		isPresent = alcCheckUnitIsPresent(kRogerBeepType);
		
	return isPresent;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCint  IsDistortionPresent()
{
	static	ALCint isPresent = kUnknownAUState;
	if (isPresent == kUnknownAUState)
		isPresent = alcCheckUnitIsPresent(kDistortionType);
		
	return isPresent;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ALC Methods
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** ALC - METHODS *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Device APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Capture Devices*****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCdevice*      ALC_APIENTRY alcCaptureOpenDevice( const ALCchar *devicename, ALCuint frequency, ALCenum format, ALCsizei buffersize )
{
#if LOG_CAPTURE_USAGE	
	DebugMessageN3("alcCaptureOpenDevice:  format %s : sample rate = %ld : buffer size = %ld", GetFormatString(format), (long int) frequency, (long int) buffersize);
#endif

	uintptr_t			newDeviceToken = 0;
	OALCaptureDevice	*newDevice = NULL;
	
	try {
		if(!IsFormatSupported(format))
			throw ((OSStatus) AL_INVALID_VALUE);
		
		if (gOALCaptureDeviceMap == NULL)
		{
			gOALCaptureDeviceMap = new OALCaptureDeviceMap ();                          // create the device map if there isn't one yet
			gCaptureDeviceMapLock = new CAGuard("OAL:CaptureLock");								// create the list guard for thread safety
		}
		
        newDeviceToken = GetNewPtrToken();                                              // get a unique token
        newDevice = new OALCaptureDevice((const char *) devicename, newDeviceToken, frequency, format, buffersize);	// create a new device object
        if (newDevice == NULL)
			throw ((OSStatus) AL_INVALID_OPERATION);

		{ 
			CAGuard::Locker locked(*gCaptureDeviceMapLock);
			gOALCaptureDeviceMap->Add(newDeviceToken, &newDevice);						// add the new device to the device map
			
		}
	}
	catch (OSStatus result) {
		DebugMessageN1("ERROR: alcCaptureOpenDevice FAILED = %s\n", alcGetString(NULL, result));
		if (newDevice) delete (newDevice);
		newDeviceToken = 0;
	}
	catch (...) {
		DebugMessage("ERROR: alcCaptureOpenDevice FAILED");
		if (newDevice) delete (newDevice);
		newDeviceToken = 0;
	}
	
	return ((ALCdevice *) newDeviceToken);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCboolean	ALC_APIENTRY alcCaptureCloseDevice( ALCdevice *device )
{
#if LOG_CAPTURE_USAGE
	DebugMessage("alcCaptureCloseDevice");
#endif
	
	try {									
		if (gOALCaptureDeviceMap == NULL)
			throw ((OSStatus)AL_INVALID_OPERATION);

		OALCaptureDevice *oalCaptureDevice = NULL;

		{
			CAGuard::Locker locked(*gCaptureDeviceMapLock);												// map operations are not thread-safe

			oalCaptureDevice = gOALCaptureDeviceMap->Get((uintptr_t) device);
			if (oalCaptureDevice == NULL)
				throw ((OSStatus) AL_INVALID_VALUE);
			
			gOALCaptureDeviceMap->Remove((uintptr_t) device);									// remove the device from the map
									
			while(oalCaptureDevice->IsInUse())
				usleep(10000);
				
			delete (oalCaptureDevice);															// destruct the device object
			
			if (gOALCaptureDeviceMap->Empty())
			{
				// there are no more devices in the map, so delete the map and create again later if needed
				delete (gOALCaptureDeviceMap);
				gOALCaptureDeviceMap = NULL;
			}
		}
		
		// if we deleted the map, we can delete the lock as well
		if (gOALCaptureDeviceMap == NULL)
			delete gCaptureDeviceMapLock;
	}
	catch (OSStatus   result) {
		DebugMessageN1("ERROR: alcCaptureCloseDevice FAILED = %s\n", alcGetString(NULL, result));
	}
    catch (...) {
		DebugMessage("ERROR: alcCaptureCloseDevice FAILED");
	}

	return AL_TRUE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API void   ALC_APIENTRY alcCaptureStart( ALCdevice *device )
{
#if LOG_CAPTURE_USAGE
	DebugMessage("alcCaptureStart");
#endif

	OALCaptureDevice *oalCaptureDevice = NULL;
	
	try {										
		oalCaptureDevice = ProtectCaptureDeviceObject ((uintptr_t) device);
        oalCaptureDevice->StartCapture();
	}
	catch (OSStatus   result) {
		DebugMessageN1("ERROR: alcCaptureStart FAILED = %s\n", alcGetString(NULL, result));
		SetDeviceError((uintptr_t) device, result);
	}
    catch (...) {
		DebugMessage("ERROR: alcCaptureStart FAILED");
		SetDeviceError((uintptr_t) device, AL_INVALID_OPERATION);
	}

	ReleaseCaptureDeviceObject(oalCaptureDevice);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API void	ALC_APIENTRY alcCaptureStop( ALCdevice *device )
{
#if LOG_CAPTURE_USAGE
	DebugMessage("alcCaptureStop");
#endif

	OALCaptureDevice *oalCaptureDevice = NULL;
	
	try {										
		oalCaptureDevice = ProtectCaptureDeviceObject ((uintptr_t) device);
        oalCaptureDevice->StopCapture();
	}
	catch (OSStatus   result) {
		DebugMessageN1("ERROR: alcCaptureStop FAILED = %s\n", alcGetString(NULL, result));
		SetDeviceError((uintptr_t) device, result);
	}
    catch (...) {
		DebugMessage("ERROR: alcCaptureStop FAILED");
		SetDeviceError((uintptr_t) device, AL_INVALID_OPERATION);
	}
	
	ReleaseCaptureDeviceObject(oalCaptureDevice);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API void	ALC_APIENTRY alcCaptureSamples( ALCdevice *device, ALCvoid *buffer, ALCsizei samples )
{
#if LOG_CAPTURE_USAGE
	DebugMessage("alcCaptureSamples");
#endif

	OALCaptureDevice *oalCaptureDevice = NULL;
	
	try {										
		oalCaptureDevice = ProtectCaptureDeviceObject ((uintptr_t) device);
        oalCaptureDevice->GetFrames(samples, (UInt8*) buffer);
	}
	catch (OSStatus   result) {
		DebugMessageN1("ERROR: alcCaptureSamples FAILED = %s\n", alcGetString(NULL, result));
		SetDeviceError((uintptr_t) device, result);
	}
    catch (...) {
		DebugMessage("ERROR: alcCaptureSamples FAILED");
		SetDeviceError((uintptr_t) device, AL_INVALID_OPERATION);
	}
	
	ReleaseCaptureDeviceObject(oalCaptureDevice);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Devices *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(const ALchar *deviceName)
{
	uintptr_t	newDeviceToken = 0;
	OALDevice	*newDevice = NULL;

#if LOG_API_USAGE
	DebugMessage("alcOpenDevice");
#endif
	
	try {
		if (Get3DMixerVersion() == kUnsupported3DMixer)
			throw -1;
		
		if (gOALDeviceMap == NULL)
		{
			gOALDeviceMap = new OALDeviceMap ();                                // create the device map if there isn't one yet
			gDeviceMapLock = new CAGuard("OAL:DeviceLock");
		}
		 		
        newDeviceToken = GetNewPtrToken();																// get a unique token
        newDevice = new OALDevice((const char *) deviceName, newDeviceToken, gRenderChannelSetting);	// create a new device object
        if (newDevice == NULL)
			throw ((OSStatus) AL_INVALID_OPERATION);
		{
			// the map is not thread-safe. We need to protect any manipulation to it
			CAGuard::Locker locked(*gDeviceMapLock);
			gOALDeviceMap->Add(newDeviceToken, &newDevice);												// add the new device to the device map
		}
	}
	catch (OSStatus result) {
		DebugMessageN1("ERROR: alcOpenDevice FAILED = %s\n", alcGetString(NULL, result));
		if (newDevice) delete (newDevice);
		SetDeviceError(gCurrentDevice, result);
		newDeviceToken = 0;
    }
	catch (...) {
		DebugMessage("ERROR: alcOpenDevice FAILED");
		if (newDevice) delete (newDevice);
		SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
		newDeviceToken = 0;
	}
		
	return ((ALCdevice *) newDeviceToken);
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCboolean   ALC_APIENTRY alcCloseDevice(ALCdevice *device)
{
#if LOG_API_USAGE
	DebugMessage("alcCloseDevice");
#endif
	ALCboolean result = ALC_TRUE;

	OALDevice *oalDevice = NULL; 
	
	try {										
		if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		oalDevice = gOALDeviceMap->Get((uintptr_t) device);	// get the requested oal device
		if (oalDevice == NULL)
			throw ((OSStatus) AL_INVALID_VALUE);
		
		{
			CAGuard::Locker locked(*gDeviceMapLock);

			gOALDeviceMap->Remove((uintptr_t) device);								// remove the device from the map
					
			DeleteContextsOfThisDevice((uintptr_t) device);

			// we cannot delete this object until all methods using it have released it
			while(oalDevice->IsInUse())
				usleep(10000);

			delete (oalDevice);														// destruct the device object
			
			if (gOALDeviceMap->Empty())
			{
				// there are no more devices in the map, so delete the map and create again later if needed
				delete (gOALDeviceMap);
				gOALDeviceMap = NULL;
			}
		}

		// if we deleted the map, we can delete the lock as well
		if (gOALDeviceMap == NULL)
			delete gDeviceMapLock;
	}
	catch (OSStatus   result) {
		DebugMessageN1("ERROR: alcCloseDevice FAILED = %s\n", alcGetString(NULL, result));
		SetDeviceError(gCurrentDevice, result);
		result = ALC_FALSE;
	}
    catch (...) {
		DebugMessage("ERROR: alcCloseDevice FAILED");
		SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
		result = ALC_FALSE;
	}
	
	return result;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCenum  ALC_APIENTRY alcGetError(ALCdevice *device)
{
#if LOG_API_USAGE
	DebugMessage("alcGetError");
#endif
	ALCenum error = noErr;
		
	try {
		if (gOALDeviceMap)
		{
			OALDevice*		oalDevice = gOALDeviceMap->Get((uintptr_t) device);
			if (oalDevice)
				error = oalDevice->GetError();
		}
		else if (gOALCaptureDeviceMap)
		{
			CAGuard::Locker locked(*gCaptureDeviceMapLock);
			
			OALCaptureDevice			*oalCaptureDevice = gOALCaptureDeviceMap->Get((uintptr_t) device);	// get the requested oal device
			if (oalCaptureDevice)
				error = oalCaptureDevice->GetError();
		}
		else
			error = ALC_INVALID_DEVICE;
	} 
	catch (OSStatus	result) {
		DebugMessage("ERROR: alcGetError FAILED: ALC_INVALID_DEVICE");
		error = ALC_INVALID_DEVICE;
	}
	catch (...) {
		DebugMessage("ERROR: alcGetError FAILED: ALC_INVALID_DEVICE");
		error = ALC_INVALID_DEVICE;
	}
		
	return error;	// don't know about this device
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Context APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Contexts *****

// There is no attribute support yet
ALC_API ALCcontext* 	ALC_APIENTRY alcCreateContext(ALCdevice *device,	const ALCint *attrList)
{
	uintptr_t		newContextToken = 0;
	OALContext		*newContext = NULL;

#if LOG_API_USAGE
	DebugMessageN1("alcCreateContext--> device = %ld", (long int) device);
#endif
	
	OALDevice	*oalDevice = NULL;

	try {
	
		oalDevice = ProtectDeviceObject ((uintptr_t) device);

		// create the context map if there isn't one yet
		if (gOALContextMap == NULL)
		{
			gOALContextMap = new OALContextMap();
			gContextMapLock = new CAGuard("OAL:ContextMapLock");
		}
		newContextToken = GetNewPtrToken();
		
		// use the attribute hint for mono/stereo sources to set gMaximumMixerBusCount
		// will only grow gMaximumMixerBusCount not shrink it below the default mixer bus count of 64
		UInt32	sourceCount = CalculateNeededMixerBusses(attrList, gMaximumMixerBusCount);
		
		// if the application has explicitly set a mixer output rate, use it,
		// otherwise just use the device's sample rate
		Float64		mixerOutputRate = gMixerOutputRate > 0.0 ? gMixerOutputRate : oalDevice->GetDeviceSampleRate();
		newContext = new OALContext(newContextToken, oalDevice, attrList, sourceCount, mixerOutputRate);
		{
			CAGuard::Locker locked(*gContextMapLock);
			gOALContextMap->Add(newContextToken, &newContext);	
		}
	}
	catch (OSStatus     result){
		DebugMessageN1("ERROR: alcCreateContext FAILED = %s\n", alcGetString(NULL, result));
		if (newContext) delete (newContext);
		SetDeviceError(gCurrentDevice, result);
		newContextToken = 0;
	}
    catch (...) {
		DebugMessage("ERROR: alcCreateContext FAILED");
		SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
		newContextToken = 0;
	}
	
	ReleaseDeviceObject(oalDevice);
	
	return ((ALCcontext *) newContextToken);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCboolean  ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessageN1("alcMakeContextCurrent--> context = %ld", (long int) context);
#endif

	OALContext		*newContext = NULL;
	OALContext		*currentContext = NULL;
	ALCboolean		result = ALC_TRUE;

	if ((uintptr_t) context == gCurrentContext)
		return ALC_TRUE;								// no change necessary, already using this context

	try {	
		if ((gOALContextMap == NULL) || (gOALDeviceMap == NULL))
            throw ((OSStatus) AL_INVALID_OPERATION);

		// get the current context if there is one
		if (gCurrentContext != 0)
			currentContext = ProtectContextObject(gCurrentContext);
			
		if (context == 0)
		{
			// Changing Current Context to NULL
			gCurrentDevice = 0;
			gCurrentContext = 0;
            
            if (currentContext)
            {
                currentContext->DisconnectMixerFromDevice();
            }
		}
		else
		{
			// Switching to a new Context
			newContext = ProtectContextObject((uintptr_t) context);
			
			uintptr_t newCurrentDeviceToken = 0;	
			{
				CAGuard::Locker locked(*gContextMapLock);
				// find the device that owns this context
				newCurrentDeviceToken = gOALContextMap->GetDeviceTokenForContext((uintptr_t) context);
			}			
			// store the new current context and device
			gCurrentDevice = newCurrentDeviceToken;
			gCurrentContext = (uintptr_t) context;
				
			newContext->ConnectMixerToDevice();
		}
		
		result = ALC_TRUE;
	}
	catch (OSStatus result) {
		DebugMessageN1("ERROR: alcMakeContextCurrent FAILED = %s\n", alcGetString(NULL, result));
		// get the Device object for this context so we can set an error code
		SetDeviceError(gCurrentDevice, result);
		result = ALC_FALSE;
	}
	catch (...) {
		DebugMessage("ERROR: alcMakeContextCurrent FAILED");
		SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
		result = ALC_FALSE;
	}
	
	ReleaseContextObject(currentContext);
	ReleaseContextObject(newContext);
	
	return result;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCvoid	  ALC_APIENTRY alcProcessContext(ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessageN1("alcProcessContext--> context = %ld", (long int) context);
#endif

	return; // no op
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCcontext* ALC_APIENTRY alcGetCurrentContext(void)
{
#if LOG_API_USAGE
	DebugMessage("alcGetCurrentContext");
#endif

	return ((ALCcontext *) gCurrentContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// find out what device the context uses
ALC_API ALCdevice*  ALC_APIENTRY alcGetContextsDevice(ALCcontext *context)
{
	uintptr_t	returnValue = 0;

#if LOG_API_USAGE
	DebugMessageN1("alcGetContextsDevice--> context = %ld", (long int) context);
#endif
		
	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);        
		{
			CAGuard::Locker locked(*gContextMapLock);
			returnValue = gOALContextMap->GetDeviceTokenForContext((uintptr_t) context);
		}
    }
    catch (OSStatus result) {
		DebugMessageN1("ERROR: alcGetContextsDevice FAILED = %s\n", alcGetString(NULL, result));
		SetDeviceError(gCurrentDevice, result);
    }
    catch (...) {
		DebugMessage("ERROR: alcGetContextsDevice FAILED");
		SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
	}
    
	return ((ALCdevice*) returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCvoid	  ALC_APIENTRY alcSuspendContext(ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessageN1("alcSuspendContext--> context = %ld", (long int) context);
#endif

	return; // no op
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCvoid	ALC_APIENTRY alcDestroyContext (ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessageN1("alcDestroyContext--> context = %ld", (long int) context);
#endif
	
	try {
        // if this was the current context, return an error
        if (gCurrentContext == (uintptr_t) context)
            throw ((OSStatus) AL_INVALID_OPERATION);

		if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		CAGuard::Locker locked(*gContextMapLock);
		
		bool			stopGraph = true;
		OALContext*		deleteThisContext = gOALContextMap->Get((uintptr_t) context);
		uintptr_t		deviceForThisContext = deleteThisContext->GetDeviceToken();
		
		if (!gOALContextMap->Remove((uintptr_t) context))	// remove from the map
			throw ((OSStatus) ALC_INVALID_CONTEXT);

		// if there are no other contexts that use the same device, the device graph can be stopped now
		for (UInt32 i = 0; i < gOALContextMap->Size(); i++)
		{
			uintptr_t		token;
			OALContext		*context = gOALContextMap->GetContextByIndex(i, token);
			if (context)
			{
				if (context->GetDeviceToken() == deviceForThisContext)
				{
					// some other context still exists that uses this device, so leave it running
					stopGraph = false;
					break;
				}
			}
		}
		
		if (stopGraph)
		{
			OALDevice		*owningDevice = ProtectDeviceObject(deviceForThisContext);
			if (owningDevice)
			{
				owningDevice->StopGraph();	// if there are no context's there is no reason for the device's graph to be running
				ReleaseDeviceObject(owningDevice);
			}
		}
        
        //only delete the context after the graph has been stopped.
        delete(deleteThisContext);
        deleteThisContext = NULL;
		
	}
	catch (OSStatus     result) {
		DebugMessageN1("ERROR: alcDestroyContext FAILED = %s\n", alcGetString(NULL, result));
		SetDeviceError(gCurrentDevice, result);
	}
    catch (...) {
		DebugMessage("ERROR: alcDestroyContext FAILED");
		SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
	}	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Other APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Other *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API const ALCchar *	ALC_APIENTRY alcGetString(ALCdevice *device, ALCenum pname)
{
#if LOG_API_USAGE
	DebugMessageN1("alcGetString-->  %s", GetALCAttributeString(pname));
#endif

	try {

		switch (pname)
		{			
			case ALC_DEFAULT_DEVICE_SPECIFIER:
				GetDefaultDeviceName(gDefaultOutputDeviceName, false);
				return gDefaultOutputDeviceName;
				break;

			case ALC_DEVICE_SPECIFIER:
			{
				GetDefaultDeviceName(gDefaultOutputDeviceName, false);
				// if a null pointer is specified, double terminate the device list
				// currently we only allow use of the default device. if this changes
				// we will need to update this mechanism to return all the devices available
				if (device == NULL)
				{
					UInt32	length = strlen(gDefaultOutputDeviceName);
					gDefaultOutputDeviceName[length + 1] = '\0'; // double terminator
				}
				return gDefaultOutputDeviceName;
			}
				break;

			case ALC_EXTENSIONS:
				// 1.1 doc: if deviceHandle is NULL for this, set error to ALC_INVALID_DEVICE
				if (device == NULL)
					throw (OSStatus) ALC_INVALID_DEVICE;
				else
					return (ALchar *) GetALCExtensionList();
				break;

			case ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER:
				GetDefaultDeviceName(gDefaultInputDeviceName, true);
				return gDefaultInputDeviceName;
				break;

			case ALC_CAPTURE_DEVICE_SPECIFIER:
			{
				GetDefaultDeviceName(gDefaultInputDeviceName, true);
				// if a null pointer is specified, double terminate the device list
				// currently we only allow use of the default device. if this changes
				// we will need to update this mechanism to return all the devices available
				if (device == NULL)
				{
					UInt32	length = strlen(gDefaultInputDeviceName);
					gDefaultInputDeviceName[length + 1] = '\0'; // double terminator
				}
				return gDefaultInputDeviceName;
			}
				break;

			case ALC_NO_ERROR:
				return (ALchar *) alNoError;
				break;
				
			case ALC_INVALID_DEVICE:
				return (ALchar *) alcErrInvalidDevice;
				break;
				
			case ALC_INVALID_CONTEXT:
				return (ALchar *) alcErrInvalidContext;
				break;
				
			case ALC_INVALID_ENUM:
				return (ALchar *) alErrInvalidEnum;
				break;
				
			case ALC_INVALID_VALUE:
				return (ALchar *) alErrInvalidValue;
				break;

			default:
				throw (OSStatus) AL_INVALID_VALUE;
				break;
		}
	}
	catch (OSStatus		result) {
		alSetError(result);
 		DebugMessageN2("ERROR: alcGetString FAILED - attribute = %s error = %s\n", GetALCAttributeString(pname), alcGetString(NULL, result));
    }
	catch (...) {
 		DebugMessageN1("ERROR: alcGetString FAILED - attribute = %s", GetALCAttributeString(pname));
	}

	return NULL;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCboolean ALC_APIENTRY alcIsExtensionPresent(ALCdevice *device, const ALCchar *extname)
{
#if LOG_API_USAGE
	DebugMessageN1("alcIsExtensionPresent-->  extension = %s", extname);
#endif
    
	ALCboolean	returnValue = AL_FALSE;
	
	if (extname == NULL)
	{
		if (device)
			SetDeviceError((uintptr_t) device, ALC_INVALID_VALUE);
		else
			SetDeviceError(gCurrentDevice, ALC_INVALID_VALUE);
	}
	else
	{
        //first compare to see if the strings match
        if(strstr(GetALCExtensionList(), extname) != NULL)
        {
            returnValue = AL_TRUE;
        } 
        else 
        {        
            //convert the extension base to upper case
            ALCchar* extbase = GetALCExtensionList();
            ALCchar* extbaseUpper = (ALCchar*)calloc(1, (strlen(extbase)+1)*sizeof(ALCchar));
            if (extbaseUpper)
            {
                for (unsigned int i=0; i < strlen(extbase); i++)
                {
                    extbaseUpper[i] = toupper(extbase[i]);
                }
                
                ALCchar* extnameUpper = (ALCchar*)calloc(1, (strlen(extname)+1)*sizeof(ALCchar));
                if (extnameUpper)
                {
                    for (unsigned int i=0; i < strlen(extname); i++)
                    {
                        extnameUpper[i] = toupper(extname[i]);
                    }

                    //compare the strings after having converted both to upper case
                    if (strstr(extbaseUpper, extnameUpper) != NULL) {
                        returnValue = AL_TRUE;
                    }
                    
                    free(extnameUpper);
                }
                
                free(extbaseUpper);
            }
        }
	}
    
	return returnValue;    // extension not present in this implementation
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API void*  ALC_APIENTRY alcGetProcAddress(ALCdevice *device, const ALCchar *funcname)
{
#if LOG_API_USAGE
	DebugMessageN1("alcGetProcAddress--> function name = %s", funcname);
#endif

	return (alGetProcAddress(funcname));
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALenum  ALC_APIENTRY alcGetEnumValue(ALCdevice *device, const ALCchar *enumName)
{
#if LOG_API_USAGE
	DebugMessageN1("alcGetEnumValue--> enum name = %s", enumName);
#endif

	return (alGetEnumValue (enumName));
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALint AL_APIENTRY alcGetInteger (ALCdevice *device, ALenum pname)
{
#if LOG_API_USAGE
	DebugMessageN2("alcGetInteger--> device = %ld attribute name = %s", (long int) device, GetALCAttributeString(pname));
#endif

	UInt32				returnValue	= 0;
	OALCaptureDevice	*oalCaptureDevice = NULL;
	OALDevice			*oalDevice = NULL;
	OALContext			*oalContext = NULL;
		
	if (gOALDeviceMap)
	{
		CAGuard::Locker locked(*gDeviceMapLock);
		oalDevice = gOALDeviceMap->Get((uintptr_t) device);	// get the requested oal device

		if (oalDevice) oalDevice->SetInUseFlag();
	}
	
	if (!oalDevice && gOALCaptureDeviceMap != NULL)
	{
		CAGuard::Locker locked(*gCaptureDeviceMapLock);	
		oalCaptureDevice = gOALCaptureDeviceMap->Get((uintptr_t) device);		// it's not an output device, look for a capture device

		if (oalCaptureDevice) oalCaptureDevice->SetInUseFlag();
	}
	
	try {

		switch (pname)
		{
			case ALC_ATTRIBUTES_SIZE:
				
				if (device == NULL || !oalDevice || device != alcGetContextsDevice((ALCcontext*) gCurrentContext))
					throw (OSStatus) ALC_INVALID_DEVICE;
				else
				{
					oalContext = ProtectContextObject(gCurrentContext);
					returnValue = oalContext->GetAttributeListSize();
				}
				break;

			case ALC_MAJOR_VERSION:
				returnValue = kMajorVersion;
				break;
				
			case ALC_MINOR_VERSION:
				returnValue = kMinorVersion;
				break;
				
			case ALC_CAPTURE_SAMPLES:
				if (oalCaptureDevice == NULL)
					throw (OSStatus) ALC_INVALID_DEVICE;
				returnValue = oalCaptureDevice->AvailableFrames();
				break;			
				
			default:
				throw (OSStatus) AL_INVALID_VALUE;
				break;
		}
	}
	catch (OSStatus		result) {
		if (oalDevice)
			oalDevice->SetError(result);
		else if(oalCaptureDevice)
			oalCaptureDevice->SetError(result);
		else 
			alSetError(result);

		DebugMessageN3("ERROR: alcGetInteger FAILED: device = %ld attribute name = %s error = %s", (long int) device, GetALCAttributeString(pname), alcGetString(NULL, result));
    }
	catch (...) {
		DebugMessageN3("ERROR: alcGetInteger FAILED: device = %ld attribute name = %s error = %s", (long int) device, GetALCAttributeString(pname), "Unknown Error");
	}
		
	ReleaseDeviceObject(oalDevice);
	ReleaseCaptureDeviceObject(oalCaptureDevice);
	ReleaseContextObject(oalContext);

	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALCvoid     ALC_APIENTRY alcGetIntegerv(ALCdevice *device, ALCenum pname, ALCsizei size, ALCint *data)
{	
#if LOG_API_USAGE
	DebugMessageN2("alcGetIntegerv--> device = %ld attribute name = %s", (long int) device, GetALCAttributeString(pname));
#endif

	// get the device
	OALCaptureDevice	*oalCaptureDevice = NULL;
	OALDevice			*oalDevice = NULL;
	OALContext			*oalContext = NULL;
	
	if (gOALDeviceMap)
	{
		CAGuard::Locker locked(*gDeviceMapLock);		
		oalDevice = gOALDeviceMap->Get((uintptr_t) device);	// get the requested oal device

		if (oalDevice) oalDevice->SetInUseFlag();
	}
	
	if (!oalDevice && gOALCaptureDeviceMap != NULL)
	{
		CAGuard::Locker locked(*gCaptureDeviceMapLock);
		oalCaptureDevice = gOALCaptureDeviceMap->Get((uintptr_t) device);		// it's not an output device, look for a capture device

		if (oalCaptureDevice) oalCaptureDevice->SetInUseFlag();
	}
	try {

		if ((data == NULL) || (size == 0))
			throw (OSStatus)ALC_INVALID_VALUE;

		switch (pname)
		{			
			case ALC_ATTRIBUTES_SIZE:
				
				if (device == NULL || !oalDevice || device != alcGetContextsDevice((ALCcontext*) gCurrentContext))
					throw (OSStatus) ALC_INVALID_DEVICE;
				else
				{
					oalContext = ProtectContextObject(gCurrentContext);
					*data = oalContext->GetAttributeListSize();
				}
				break;
				
			case ALC_ALL_ATTRIBUTES:
				
				if (device == NULL || !oalDevice || device != alcGetContextsDevice((ALCcontext*) gCurrentContext))
					throw (OSStatus) ALC_INVALID_DEVICE;
				else
				{
					oalContext = ProtectContextObject(gCurrentContext);
					if (size < (ALCsizei) oalContext->GetAttributeListSize())
						throw (OSStatus) ALC_INVALID_VALUE;
					
					oalContext->CopyAttributeList(data);
				}
				break;
				
			case ALC_MAJOR_VERSION:
				*data = kMajorVersion;
				break;
				
			case ALC_MINOR_VERSION:
				*data = kMinorVersion;
				break;
			case ALC_CAPTURE_SAMPLES:
				if (oalCaptureDevice == NULL)
					throw (OSStatus) ALC_INVALID_DEVICE;
				*data = oalCaptureDevice->AvailableFrames();
				break;

			default:
				throw (OSStatus) AL_INVALID_VALUE;
				break;
		}
	}
	catch (OSStatus		result) {
		if (oalDevice)
			oalDevice->SetError(result);
		else if(oalCaptureDevice)
			oalCaptureDevice->SetError(result);
		else 
			alSetError(result);

		DebugMessageN3("ERROR: alcGetInteger FAILED: device = %ld attribute name = %s error = %s", (long int) device, GetALCAttributeString(pname), alcGetString(NULL, result));
    }
	catch (...) {
		DebugMessageN3("ERROR: alcGetInteger FAILED: device = %ld attribute name = %s error = %s", (long int) device, GetALCAttributeString(pname), "Unknown Error");
	}
	
	ReleaseDeviceObject(oalDevice);
	ReleaseCaptureDeviceObject(oalCaptureDevice);
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AL Methods
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** AL - METHODS *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALenum AL_APIENTRY alGetError ()
{
    ALenum	error = AL_NO_ERROR;

#if LOG_API_USAGE && LOG_EXTRAS
	DebugMessage("alGetError");
#endif

	if (gCurrentError != AL_NO_ERROR)
    {
#if LOG_ERRORS
		DebugMessageN1("alGetError: error = 0x%X\n", (uint) gCurrentError);
#endif
		error = gCurrentError;
		gCurrentError = AL_NO_ERROR;

		// this call should also clear the error on the current device as well
    	if (gOALDeviceMap) 
    	{
			OALDevice		*device = gOALDeviceMap->Get((UInt32) gCurrentDevice);
			if (device)
				device->SetError(AL_NO_ERROR);
		}
	}

	return (error);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Buffer APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Buffers *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGenBuffers(ALsizei n, ALuint *bids)
{
#if LOG_BUFFER_USAGE
	DebugMessageN1("alGenBuffers--> requested buffers = %ld", (long int) n);
#endif
	
	try {
		if (n < 0)
            throw ((OSStatus) AL_INVALID_VALUE);

        InitializeBufferMap();
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		CAGuard::Locker locked(*gBufferMapLock);

		CleanUpDeadBufferList();		// take the opportunity to clean up the dead list
		
		if ((n + gOALBufferMap->Size() > AL_MAXBUFFERS) || (n > AL_MAXBUFFERS))
		{
			DebugMessageN2("alGenBuffers ERROR --> requested buffers:gOALBufferMap->Size() = %ld:%ld", (long int) n, (long int) gOALBufferMap->Size());
            throw ((OSStatus) AL_INVALID_VALUE);
		}
		
        for (UInt32	i = 0; i < (UInt32) n; i++)
        {
			ALuint	newBufferToken = GetNewToken();		// get a unique token

			OALBuffer	*newBuffer = new OALBuffer (newBufferToken);
			
            gOALBufferMap->Add(newBufferToken, &newBuffer);			// add the new buffer to the buffer map
            bids[i] = newBufferToken;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN2("ERROR: alGenBuffers FAILED: requested buffers = %ld error %s", (long int) n, alGetString(result));
        alSetError (result);
	}
    catch (...) {
		DebugMessageN1("ERROR: alGenBuffers FAILED: requested buffers = %ld", (long int) n);
        alSetError(AL_INVALID_OPERATION);
	}

#if LOG_BUFFER_USAGE
	printf("alGenBuffers--> (%ld) ", (long int) n);
	for (UInt32	i = 0; i < (UInt32) n; i++) {
		printf("%ld, ", (long int) bids[i]);
	}
	printf("\n");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alDeleteBuffers( ALsizei n, const ALuint* bids )
{
#if LOG_BUFFER_USAGE
	printf("alDeleteBuffers--> (%ld) ", (long int) n);
	for (UInt32	i = 0; i < (UInt32) n; i++) {
		printf("%ld, ", (long int) bids[i]);
	}
	printf("\n");
#endif
	if (n == 0)
		return; // NOP
	
	try {
        if (gOALBufferMap == NULL)
        {
            DebugMessage("alDeleteBuffers: gOALBufferMap == NULL");
            throw ((OSStatus) AL_INVALID_VALUE);   
        }
		else if (n < 0)
		{
			DebugMessage("alDeleteBuffers: Invalid number of buffers");
			throw ((OSStatus) AL_INVALID_VALUE);
		}

		CAGuard::Locker locked(*gBufferMapLock);

		CleanUpDeadBufferList();		// take the opportunity to clean up the dead list
        			
		// see if any of the buffers are attached to a source or are invalid names
		for (UInt32 i = 0; i < (UInt32) n; i++)
		{
			// don't bother verifying the NONE buffer, it won't be deleted anyway
			if (bids[i] != AL_NONE)
			{
				OALBuffer	*oalBuffer = gOALBufferMap->Get(bids[i]);
				if (oalBuffer == NULL)
				{
					DebugMessageN1("alDeleteBuffers: oalBuffer == NULL, bid = %ld", (long int) bids[i]);
					throw ((OSStatus) AL_INVALID_NAME);			// the buffer is invalid
				}
				else if (!oalBuffer->CanBeRemovedFromBufferMap())
				{
					DebugMessageN1("alDeleteBuffers: oalBuffer cannot currently be removed, a source is still attached, bid = %ld", (long int) bids[i]);
					throw ((OSStatus) AL_INVALID_OPERATION);    // the buffer is attached to a source so set an error and bail
				}
			}
		}
	
		// All the buffers are OK'd for deletion, so delete them now
		for (UInt32 i = 0; i < (UInt32) n; i++)
		{
			// do not delete the NONE buffer at the beginning of the map
			if (bids[i] != AL_NONE)
			{
				OALBuffer	*buffer = gOALBufferMap->Get((UInt32) bids[i]);
				if (buffer->IsPurgable())
				{
					// delete it right now, it's not attached
					gOALBufferMap->Remove((UInt32) bids[i]);
					delete (buffer);
				}
				else
				{
					// The buffer is currently attached to a source that is transitioning to stop
					// it can be deleted later. By removing it from the map, it can't be attached to another source before
					// it has a chance to be deleted
					gDeadOALBufferMap->Add(buffer->GetToken(), &buffer);	// add this buffer object to a dead list that can be cleaned up later
					gOALBufferMap->Remove((UInt32) bids[i]);				// remove it from the good list so it won't be used again
				}
			}
		}
	}
    catch (OSStatus     result) {
		DebugMessageN1("ERROR: alDeleteBuffers FAILED = %s", alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessage("ERROR: alDeleteBuffers FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALboolean AL_APIENTRY alIsBuffer(ALuint bid)
{
#if LOG_BUFFER_USAGE
	//DebugMessageN1("alIsBuffer--> buffer %ld", (long int) bid);
#endif

	ALboolean isBuffer = AL_FALSE;
	
	try {
		isBuffer = IsValidBufferObject(bid);		
    }
    catch (OSStatus     result) {
		DebugMessageN2("ERROR: alIsBuffer FAILED: buffer = %ld error = %s", (long int) bid, alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessageN1("ERROR: alIsBuffer FAILED: buffer = %ld", (long int) bid);
        alSetError(AL_INVALID_OPERATION);
	}

    return isBuffer;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alBufferData(	ALuint			bid,
										ALenum			format,
										const ALvoid*	data,
										ALsizei			size,
										ALsizei			freq )
{
#if LOG_BUFFER_USAGE
	DebugMessageN4("alBufferData-->  buffer %ld : %s : %ld bytes : %ldHz", (long int) bid, GetFormatString(format), (long int) size, (long int) freq);
#endif

	OALBuffer *oalBuffer = NULL;
	
	try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   
		
		oalBuffer = ProtectBufferObject(bid);
        if (data==NULL) throw ((OSStatus) AL_INVALID_VALUE);
        if (size<=0) throw ((OSStatus) AL_INVALID_VALUE);
				
		oalBuffer->AddAudioData((char*)data, size, format, freq, gConvertBufferNow); // should also check for a valid format IsFormatSupported()
    }
    catch (OSStatus     result) {
		DebugMessageN5("ERROR: alBufferData FAILED: buffer %ld : %s : %ld bytes : %ldHz error = %s", (long int) bid, GetFormatString(format), (long int) size, (long int) freq,  alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessageN4("ERROR: alBufferData FAILED: buffer %ld : %s : %ld bytes : %ldHz", (long int) bid, GetFormatString(format), (long int) size, (long int) freq);
        alSetError(AL_INVALID_OPERATION);
	}

	ReleaseBufferObject(oalBuffer);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alBufferf( ALuint bid, ALenum pname, ALfloat value )
{
#if LOG_BUFFER_USAGE
	DebugMessage("alBufferf--> there are currently no valid enums for this API");
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alBuffer3f( ALuint bid, ALenum pname, ALfloat value1, ALfloat value2, ALfloat value3 )
{
#if LOG_BUFFER_USAGE
	DebugMessage("alBuffer3f--> there are currently no valid enums for this API");
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alBufferfv( ALuint bid, ALenum pname, const ALfloat* values )
{
#if LOG_BUFFER_USAGE
	DebugMessage("alBufferfv--> there are currently no valid enums for this API");
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alBufferi( ALuint bid, ALenum pname, ALint value )
{
#if LOG_BUFFER_USAGE
	DebugMessage("alBufferi--> there are currently no valid enums for this API");
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alBuffer3i( ALuint bid, ALenum pname, ALint value1, ALint value2, ALint value3 )
{
#if LOG_BUFFER_USAGE
	DebugMessage("alBuffer3i--> there are currently no valid enums for this API");
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alBufferiv( ALuint bid, ALenum pname, const ALint* values )
{
#if LOG_BUFFER_USAGE
	DebugMessage("alBufferiv--> there are currently no valid enums for this API");
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetBufferf (ALuint bid, ALenum pname, ALfloat *value)
{
#if LOG_BUFFER_USAGE
	DebugMessageN2("alGetBufferf--> buffer %ld : property = %s", (long int) bid, GetALAttributeString(pname));
#endif
	OALBuffer	*oalBuffer = NULL;

    try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

		oalBuffer = ProtectBufferObject(bid);
			
        switch (pname)
        {
            case AL_FREQUENCY:
                *value = oalBuffer->GetSampleRate();
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
    }
    catch (OSStatus result) {
		DebugMessageN3("ERROR: alGetBufferf FAILED: buffer %ld : property = %s error = %s", (long int) bid, GetALAttributeString(pname), alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessageN2("ERROR: alGetBufferf FAILED: buffer %ld : property = %s", (long int) bid, GetALAttributeString(pname));
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseBufferObject(oalBuffer);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetBuffer3f( ALuint bid, ALenum pname, ALfloat* value1, ALfloat* value2, ALfloat* value3)
{
#if LOG_BUFFER_USAGE
	DebugMessageN2("alGetBuffer3f--> buffer %ld : property = %s", (long int) bid, GetALAttributeString(pname));
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetBufferfv( ALuint bid, ALenum pname, ALfloat* values )
{
#if LOG_BUFFER_USAGE
	DebugMessageN2("alGetBufferfv--> buffer %ld : property = %s", (long int) bid, GetALAttributeString(pname));
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetBufferi(ALuint bid, ALenum pname, ALint *value)
{
#if LOG_BUFFER_USAGE
	DebugMessageN2("alGetBufferi--> buffer %ld : property = %s", (long int) bid, GetALAttributeString(pname));
#endif

	OALBuffer	*oalBuffer = NULL;
    
	try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

		oalBuffer = ProtectBufferObject(bid);

        switch (pname)
        {
            case AL_FREQUENCY:
                *value = (UInt32) oalBuffer->GetSampleRate();
                break;
            case AL_BITS:
                *value = oalBuffer->GetPreConvertedBitsPerChannel();
                break;
            case AL_CHANNELS:
                *value = oalBuffer->GetNumberChannels();
                break;
            case AL_SIZE:
                *value = oalBuffer->GetPreConvertedDataSize();
                break;
            default:
                *value = 0;
				alSetError(AL_INVALID_ENUM);
                break;
        }        
    }
    catch (OSStatus result) {
		DebugMessageN3("ERROR: alGetBufferi FAILED: buffer = %ld property = %s error = %s", (long int) bid, GetALAttributeString(pname), alGetString(result));
		*value = 0;
		alSetError(result);
    }
    catch (...) {
		DebugMessageN2("ERROR: alGetBufferi FAILED: buffer = %ld property = %s", (long int) bid, GetALAttributeString(pname));
		*value = 0;
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseBufferObject(oalBuffer);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetBuffer3i( ALuint bid, ALenum pname, ALint* value1, ALint* value2, ALint* value3)
{
#if LOG_BUFFER_USAGE
	DebugMessageN2("alGetBuffer3i--> buffer %ld : property = %s", (long int) bid, GetALAttributeString(pname));
#endif
	// there are currently no valid enums for this API
	alSetError(AL_INVALID_ENUM);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetBufferiv( ALuint bid, ALenum pname, ALint* values )
{
#if LOG_BUFFER_USAGE
	DebugMessageN2("alGetBufferi--> buffer %ld : property = %s", (long int) bid, GetALAttributeString(pname));
#endif

	OALBuffer	*oalBuffer = NULL;

    try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

		oalBuffer = ProtectBufferObject(bid);		
		
       switch (pname)
        {
            case AL_FREQUENCY:
                *values = (UInt32) oalBuffer->GetSampleRate();
                break;
            case AL_BITS:
                *values = oalBuffer->GetPreConvertedBitsPerChannel();
                break;
            case AL_CHANNELS:
                *values = oalBuffer->GetNumberChannels();
                break;
            case AL_SIZE:
                *values = oalBuffer->GetPreConvertedDataSize();
                break;
            default:
                *values = 0;
				alSetError(AL_INVALID_ENUM);
                break;
        }        
    }
    catch (OSStatus result) {
		DebugMessageN3("ERROR: alGetBufferiv FAILED: buffer = %ld property = %s error = %s", (long int) bid, GetALAttributeString(pname), alGetString(result));
		*values = 0;
		alSetError(result);
    }
    catch (...) {
		DebugMessageN2("ERROR: alGetBufferiv FAILED: buffer = %ld property = %s", (long int) bid, GetALAttributeString(pname));
		*values = 0;
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseBufferObject(oalBuffer);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Source APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Sources *****	
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid  AL_APIENTRY alGenSources(ALsizei n, ALuint *sids)
{
#if LOG_SOURCE_USAGE
	DebugMessageN1("alGenSources--> requested sources = %ld", (long int) n);
#endif

	if (n == 0)
		return; // NOP

	UInt32      i = 0,
                count = 0;
				
	OALContext *oalContext = NULL;

	try {
		if (n < 0)
            throw ((OSStatus) AL_INVALID_VALUE);

        if ((n > AL_MAXSOURCES) || (sids == NULL))
            throw ((OSStatus) AL_INVALID_VALUE);
        
		oalContext = ProtectContextObject(gCurrentContext);
        
		for (i = 0; i < (UInt32) n; i++)
        {
            ALuint	newToken = GetNewToken();		// get a unique token
            
            oalContext->AddSource(newToken);		// add this source to the context
            sids[i] = newToken; 					// return the source token
            count++;
        }
	}
	catch (OSStatus     result){
		DebugMessageN2("ERROR: alGenSources FAILED: source count = %ld error = %s", (long int) n, alGetString(result));
		// some of the sources could not be created, so delete the ones that were and return none
		alSetError(result);
		alDeleteSources(i, sids);
		for (i = 0; i < count; i++)
			sids[i] = 0;
	}
    catch (...) {
		DebugMessageN1("ERROR: alGenSources FAILED: source count = %ld", (long int) n);
        alSetError(AL_INVALID_OPERATION);
		// some of the sources could not be created, so delete the ones that were and return none
		alDeleteSources(i, sids);
		for (i = 0; i < count; i++)
			sids[i] = 0;
	}	

	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alDeleteSources( ALsizei n, const ALuint* sids )
{
#if LOG_SOURCE_USAGE
	//DebugMessageN1("alDeleteSources: count = %ld", (long int) n);
	printf("alDeleteSources--> (%ld) ", (long int) n);
	for (UInt32	i = 0; i < (UInt32) n; i++) {
		printf("%ld, ", (long int) sids[i]);
	}
	printf("\n");
#endif

	if (n == 0)
		return; // NOP

	OALContext *oalContext = NULL;
	
	try {
		oalContext = ProtectContextObject(gCurrentContext);

        if ((UInt32) n > oalContext->GetSourceCount())
            throw ((OSStatus) AL_INVALID_VALUE);
		else if (n < 0)
			throw ((OSStatus) AL_INVALID_VALUE);
        
        for (UInt32 i = 0; i < (UInt32) n; i++)
        {
			alSourceStop (sids[i]);
            oalContext->RemoveSource(sids[i]);
        }
	}
	catch (OSStatus     result) {
		DebugMessageN2("ERROR: alDeleteSources FAILED: source count = %ld error = %s", (long int) n, alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN1("ERROR: alDeleteSources FAILED: source count = %ld", (long int) n);
        alSetError(AL_INVALID_OPERATION);
	}

	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALboolean  AL_APIENTRY alIsSource(ALuint sid)
{
#if LOG_SOURCE_USAGE
	DebugMessageN1("alIsSource--> source %ld", (long int) sid);
#endif
	ALboolean isSource = AL_FALSE;
	OALSource *oalSource = NULL;
	
	try {		
		oalSource = ProtectSourceObjectInCurrentContext(sid);
		if (oalSource)
			isSource = AL_TRUE;
	}
	catch (OSStatus     result) {
		DebugMessageN2("ERROR: alIsSource FAILED: source = %ld error = %s", (long int) sid, alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN1("ERROR: alIsSource FAILED: source = %ld", (long int) sid);
        alSetError(AL_INVALID_OPERATION);
	}

	ReleaseSourceObject(oalSource);
	
	return isSource;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourcef (ALuint sid, ALenum pname, ALfloat value)
{
#if LOG_SOURCE_USAGE
	DebugMessageN3("alSourcef--> source %ld : %s : value = %.2f", (long int) sid, GetALAttributeString(pname), value);
#endif
		
	OALSource *oalSource = NULL;
	
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch (pname) 
        {
			// Source ONLY Attributes
            case AL_MIN_GAIN:
                oalSource->SetMinGain(value);
                break;
            case AL_MAX_GAIN:
                oalSource->SetMaxGain(value);
                break;
            case AL_REFERENCE_DISTANCE:
                oalSource->SetReferenceDistance(value);
                break;
            case AL_ROLLOFF_FACTOR:
                oalSource->SetRollOffFactor(value);
                break;
            case AL_MAX_DISTANCE:
                oalSource->SetMaxDistance(value);
                break;
            case AL_PITCH:
                oalSource->SetPitch(value);
                break;
            case AL_CONE_INNER_ANGLE:
                oalSource->SetConeInnerAngle(value);
                break;
            case AL_CONE_OUTER_ANGLE:
                oalSource->SetConeOuterAngle(value);
                break;
            case AL_CONE_OUTER_GAIN:
				oalSource->SetConeOuterGain(value);
                break;
            case AL_SEC_OFFSET:
                oalSource->SetQueueOffset(kSecondsOffset, value);
                break;
            case AL_SAMPLE_OFFSET:
                oalSource->SetQueueOffset(kSampleOffset, value);
                break;
            case AL_BYTE_OFFSET:
                oalSource->SetQueueOffset(kByteOffset, value);
                break;

			// Source & Listener Attributes
            case AL_GAIN:
                oalSource->SetGain(value);
                break;
			
            default:
                alSetError(AL_INVALID_OPERATION);
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN4("ERROR alSourcef FAILED: source %ld : property = %s : value = %.f2 : error = %s", (long int) sid, GetALAttributeString(pname), value, alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN4("ERROR alSourcef FAILED: source %ld : property = %s : value = %.f2 : error = %s", (long int) sid, GetALAttributeString(pname), value, alGetString(-1));
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourcefv (ALuint sid, ALenum pname, const ALfloat *values)
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alSourcefv--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif
	
	OALSource *oalSource = NULL;
			
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch(pname) 
        {
			// Source ONLY Attributes
            case AL_MIN_GAIN:
                oalSource->SetMinGain(*values);
                break;
            case AL_MAX_GAIN:
                oalSource->SetMaxGain(*values);
                break;
            case AL_REFERENCE_DISTANCE:
                oalSource->SetReferenceDistance(*values);
                break;
            case AL_ROLLOFF_FACTOR:
                if (*values < 0.0f) 
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetRollOffFactor(*values);
                break;
            case AL_MAX_DISTANCE:
                oalSource->SetMaxDistance(*values);
                break;
            case AL_PITCH:
                oalSource->SetPitch(*values);
                break;
            case AL_DIRECTION:
                oalSource->SetDirection(values[0], values[1], values[2]);
                break;
            case AL_CONE_INNER_ANGLE:
                oalSource->SetConeInnerAngle(*values);
                break;
            case AL_CONE_OUTER_ANGLE:
                oalSource->SetConeOuterAngle(*values);
                break;
            case AL_CONE_OUTER_GAIN:
				oalSource->SetConeOuterGain(*values);
                break;
            case AL_SEC_OFFSET:
                oalSource->SetQueueOffset(kSecondsOffset, values[0]);
                break;
            case AL_SAMPLE_OFFSET:
                oalSource->SetQueueOffset(kSampleOffset, values[0]);
                break;
            case AL_BYTE_OFFSET:
                oalSource->SetQueueOffset(kByteOffset, values[0]);
                break;
			
			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->SetPosition(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalSource->SetVelocity(values[0], values[1], values[2]);
                break;
            case AL_GAIN:
                oalSource->SetGain(*values);
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch(OSStatus      result) {
		DebugMessageN3("ERROR alSourcefv FAILED: source = %ld property = %s result = %s\n", (long int) sid, GetALAttributeString(pname), alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN3("ERROR alSourcefv: FAILED : property : result %ld : %s : %s\n", (long int) sid, GetALAttributeString(pname), alGetString(-1));
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSource3f (ALuint sid, ALenum pname, ALfloat v1, ALfloat v2, ALfloat v3)
{
#if LOG_SOURCE_USAGE
	DebugMessageN5("alSource3f--> source %ld : %s : values = %.2f:%.2f:%.2f", (long int) sid, GetALAttributeString(pname), v1, v2, v3);
#endif
		
	OALSource *oalSource = NULL;
	
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);
		
        switch (pname) 
        {
			// Source ONLY Attributes
            case AL_DIRECTION:
                oalSource->SetDirection(v1, v2, v3);
                break;

			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->SetPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalSource->SetVelocity(v1, v2, v3);
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN6("ERROR: alSource3f FAILED: source %ld : %s : values = %.f2:%.f2:%.f2 error = %s", (long int) sid, GetALAttributeString(pname), v1, v2, v3, alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN5("ERROR: alSource3f FAILED: source %ld : %s : values = %.f2:%.f2:%.f2", (long int) sid, GetALAttributeString(pname), v1, v2, v3);
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourcei (ALuint sid, ALenum pname, ALint value)
{
#if LOG_SOURCE_USAGE
	DebugMessageN3("alSourcei--> source %ld : %s : value = %ld", (long int) sid, GetALAttributeString(pname), (long int)value);
#endif
		
	OALSource *oalSource = NULL;
	
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

		switch (pname) 
        {
			// Source ONLY Attributes
            case AL_SOURCE_RELATIVE:
				oalSource->SetSourceRelative(value);
                break;
            case AL_LOOPING:
                oalSource->SetLooping(value);
                break;
            case AL_BUFFER:
                {
					// The source type must first be checked for static or streaming
					if (oalSource->GetSourceType() == AL_STREAMING && value != 0)
						throw ((OSStatus) AL_INVALID_OPERATION);
					try {
						if (IsValidBufferObject(value))
						{
							// if no buffers have been made yet but IsValidBufferObject() is true, then this is a AL_NONE buffer
							if (gOALBufferMap)
							{
								CAGuard::Locker locked(*gBufferMapLock);
								oalSource->SetBuffer(value, gOALBufferMap->Get((UInt32) value));
							}
						}
						else
							throw ((OSStatus) AL_INVALID_VALUE);	// per the OpenAL 1.1 spec
					}
					catch (OSStatus result) {
						if (result == AL_INVALID_NAME)
							result = AL_INVALID_VALUE;	// convert result from IsValidBufferObject() when buffer is not primary object
						throw result;
					}
                } 
                break;
            case AL_REFERENCE_DISTANCE:
                oalSource->SetReferenceDistance(value);
                break;
            case AL_ROLLOFF_FACTOR:
                oalSource->SetRollOffFactor(value);
                break;
            case AL_MAX_DISTANCE:
                oalSource->SetMaxDistance(value);
                break;
			case AL_CONE_INNER_ANGLE:
                oalSource->SetConeInnerAngle(value);
                break;
            case AL_CONE_OUTER_ANGLE:
                oalSource->SetConeOuterAngle(value);
                break;
            case AL_CONE_OUTER_GAIN:
				oalSource->SetConeOuterGain(value);
                break;
            case AL_SEC_OFFSET:
                oalSource->SetQueueOffset(kSecondsOffset, value);
                break;
            case AL_SAMPLE_OFFSET:
                oalSource->SetQueueOffset(kSampleOffset, value);
                break;
            case AL_BYTE_OFFSET:
                oalSource->SetQueueOffset(kByteOffset, value);
                break;
            
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN4("ERROR: alSourcei FAILED - sid:pname:value:result %ld:%s:%ld:%s", (long int) sid, GetALAttributeString( pname), (long int) value, alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN3("ERROR: alSourcei FAILED - sid:pname:value %ld:%s:%ld", (long int) sid, GetALAttributeString( pname), (long int) value);
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alSourceiv( ALuint sid, ALenum pname, const ALint* values )
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alSourceiv--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif
		
	OALSource *oalSource = NULL;
	
	try {
		if (values == NULL)
			throw ((OSStatus) AL_INVALID_VALUE);

		oalSource = ProtectSourceObjectInCurrentContext(sid);
		switch (pname) 
        {
			// Source ONLY Attributes
            case AL_SOURCE_RELATIVE:
                if ((*values == AL_FALSE) || (*values == AL_TRUE))
                    oalSource->SetSourceRelative(*values);
                else
                    throw ((OSStatus) AL_INVALID_VALUE);
                break;
            case AL_LOOPING:
                oalSource->SetLooping(*values);
                break;
            case AL_BUFFER:
			{
				// The source type must first be checked for static or streaming
				if (oalSource->GetSourceType() == AL_STREAMING)
					throw ((OSStatus) AL_INVALID_OPERATION);

				if (IsValidBufferObject(*values))
				{
					// if no buffers have been made yet but IsValidBufferObject() is true, then this is a AL_NONE buffer
					if (gOALBufferMap)
					{
						CAGuard::Locker locked(*gBufferMapLock);
						oalSource->SetBuffer(*values, gOALBufferMap->Get((UInt32) *values));
					}
				}
				else
					throw ((OSStatus) AL_INVALID_OPERATION);
			} 
                break;
			case AL_REFERENCE_DISTANCE:
                oalSource->SetReferenceDistance(*values);
                break;
            case AL_ROLLOFF_FACTOR:
                oalSource->SetRollOffFactor(*values);
                break;
            case AL_MAX_DISTANCE:
                oalSource->SetMaxDistance(*values);
                break;
            case AL_DIRECTION:
                oalSource->SetDirection(values[0], values[1], values[2]);
                break;
			case AL_CONE_INNER_ANGLE:
                oalSource->SetConeInnerAngle(*values);
                break;
            case AL_CONE_OUTER_ANGLE:
                oalSource->SetConeOuterAngle(*values);
                break;
			case AL_SEC_OFFSET:
                oalSource->SetQueueOffset(kSecondsOffset, *values);
                break;
            case AL_SAMPLE_OFFSET:
                oalSource->SetQueueOffset(kSampleOffset, *values);
                break;
            case AL_BYTE_OFFSET:
                oalSource->SetQueueOffset(kByteOffset, *values);
                break;
			
			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->SetPosition(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalSource->SetVelocity(values[0], values[1], values[2]);
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
		}
	}
	catch (OSStatus      result) {
		DebugMessageN3("ERROR: alSourcei FAILED: source %ld : %s error = %s", (long int) sid, GetALAttributeString(pname), alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN2("ERROR: alSourcei FAILED: source %ld : %s", (long int) sid, GetALAttributeString(pname));
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alSource3i( ALuint sid, ALenum pname, ALint v1, ALint v2, ALint v3 )
{
#if LOG_SOURCE_USAGE
	DebugMessageN5("alSource3i--> source %ld : %s : values = %ld:%ld:%ld", (long int) sid, GetALAttributeString(pname), (long int)v1, (long int)v2, (long int)v3);
#endif
	
	OALSource *oalSource = NULL;
	
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch (pname) 
        {
			// Source ONLY Attributes
            case AL_DIRECTION:
                oalSource->SetDirection(v1, v2, v3);
                break;

			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->SetPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalSource->SetVelocity(v1, v2, v3);
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN6("ERROR: alSource3f FAILED: source %ld : %s : values = %ld:%ld:%ld error = %s", (long int) sid, GetALAttributeString(pname), (long int)v1, (long int)v2, (long int)v3, alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessageN5("ERROR: alSource3f FAILED: source %ld : %s : values = %ld:%ld:%ld", (long int) sid, GetALAttributeString(pname), (long int)v1, (long int)v2, (long int)v3);
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetSourcef (ALuint sid, ALenum pname, ALfloat *value)
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alGetSourcef--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif
	
	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch (pname) 
        {
			// Source ONLY Attributes
            case AL_MIN_GAIN:
                *value = oalSource->GetMinGain();
                break;
            case AL_MAX_GAIN:
                *value = oalSource->GetMaxGain();
                break;
            case AL_REFERENCE_DISTANCE:
                *value = oalSource->GetReferenceDistance();
                break;
            case AL_ROLLOFF_FACTOR:
                *value = oalSource->GetRollOffFactor();
                break;
            case AL_MAX_DISTANCE:
                *value = oalSource->GetMaxDistance();
                break;
            case AL_PITCH:
                *value = oalSource->GetPitch();
                break;
            case AL_CONE_INNER_ANGLE:
                *value = oalSource->GetConeInnerAngle();
                break;
            case AL_CONE_OUTER_ANGLE:
                *value = oalSource->GetConeOuterAngle();
                break;
            case AL_CONE_OUTER_GAIN:
                *value = oalSource->GetConeOuterGain();
                break;
            case AL_SEC_OFFSET:
                *value = oalSource->GetQueueOffsetSecondsFloat();
                break;
            case AL_SAMPLE_OFFSET:
                *value = oalSource->GetQueueOffset(kSampleOffset);
                break;
            case AL_BYTE_OFFSET:
                *value = oalSource->GetQueueOffset(kByteOffset);
                break;

			// Source & Listener Attributes
            case AL_GAIN:
                *value = oalSource->GetGain();
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alGetSourcef FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alGetSourcef FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetSourcefv (ALuint sid, ALenum pname, ALfloat *values)
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alGetSourcefv--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif

	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch(pname) 
        {
			// Source ONLY Attributes
            case AL_MIN_GAIN:
                *values = oalSource->GetMinGain();
                break;
            case AL_MAX_GAIN:
                *values = oalSource->GetMaxGain();
                break;
            case AL_REFERENCE_DISTANCE:
                *values = oalSource->GetReferenceDistance();
                break;
            case AL_ROLLOFF_FACTOR:
                *values = oalSource->GetRollOffFactor();
                break;
            case AL_MAX_DISTANCE:
                *values = oalSource->GetMaxDistance();
                break;
            case AL_PITCH:
                *values = oalSource->GetPitch();
                break;
            case AL_DIRECTION:
                oalSource->GetDirection(values[0], values[1], values[2]);
                break;
            case AL_CONE_INNER_ANGLE:
                *values = oalSource->GetConeInnerAngle();
                break;
            case AL_CONE_OUTER_ANGLE:
                *values = oalSource->GetConeOuterAngle();
                break;
            case AL_CONE_OUTER_GAIN:
                *values = oalSource->GetConeOuterGain();
                break;
            case AL_SEC_OFFSET:
                *values = oalSource->GetQueueOffsetSecondsFloat();
                break;
            case AL_SAMPLE_OFFSET:
                *values = oalSource->GetQueueOffset(kSampleOffset);
                break;
            case AL_BYTE_OFFSET:
                *values = oalSource->GetQueueOffset(kByteOffset);
                break;

			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->GetPosition(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalSource->GetVelocity(values[0], values[1], values[2]);
                break;
            case AL_GAIN:
                *values = oalSource->GetGain();
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alGetSourcefv FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alGetSourcefv FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetSource3f (ALuint sid, ALenum pname, ALfloat *v1, ALfloat *v2, ALfloat *v3)
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alGetSource3f--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif

	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch (pname) 
        {
			// Source ONLY Attributes
            case AL_DIRECTION:
                oalSource->GetDirection(*v1, *v2, *v3);
                break;

			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->GetPosition(*v1, *v2, *v3);
                break;
            case AL_VELOCITY:
                oalSource->GetVelocity(*v1, *v2, *v3);
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alGetSource3f FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alGetSource3f FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetSourcei (ALuint sid, ALenum pname, ALint *value)
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alGetSourcei--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif

	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch (pname) 
        {
			// Source ONLY Attributes
            case AL_SOURCE_RELATIVE:
                *value = oalSource->GetSourceRelative();
                break;
            case AL_SOURCE_TYPE:
                *value = oalSource->GetSourceType();
                break;
            case AL_LOOPING:
                *value = oalSource->GetLooping();
                break;
            case AL_BUFFER:
                *value = oalSource->GetBuffer();
                break;
            case AL_BUFFERS_QUEUED:
                *value = oalSource->GetQLength();
                break;
            case AL_BUFFERS_PROCESSED:
                *value = oalSource->GetBuffersProcessed();
                break;
            case AL_REFERENCE_DISTANCE:
                *value = (ALint) oalSource->GetReferenceDistance();
                break;
            case AL_ROLLOFF_FACTOR:
                *value = (ALint) oalSource->GetRollOffFactor();
                break;
            case AL_MAX_DISTANCE:
                *value = (ALint) oalSource->GetMaxDistance();
                break;
            case AL_CONE_INNER_ANGLE:
                *value = (UInt32) oalSource->GetConeInnerAngle();
                break;
            case AL_CONE_OUTER_ANGLE:
                *value = (UInt32) oalSource->GetConeOuterAngle();
                break;
            case AL_CONE_OUTER_GAIN:
                *value = (UInt32) oalSource->GetConeOuterGain();
                break;
            case AL_SEC_OFFSET:
                *value = oalSource->GetQueueOffset(kSecondsOffset);
                break;
            case AL_SAMPLE_OFFSET:
                *value = oalSource->GetQueueOffset(kSampleOffset);
                break;
            case AL_BYTE_OFFSET:
                *value = oalSource->GetQueueOffset(kByteOffset);
                break;
            case AL_SOURCE_STATE:
                *value = oalSource->GetState();
                break;
            
			default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alGetSourcei FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alGetSourcei FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetSourceiv( ALuint sid,  ALenum pname, ALint* values )
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alGetSourceiv--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif
	
	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch(pname) 
        {
			// Source ONLY Attributes
            case AL_SOURCE_RELATIVE:
                *values = oalSource->GetSourceRelative();
                break;
            case AL_SOURCE_TYPE:
                *values = oalSource->GetSourceType();
                break;
            case AL_LOOPING:
                *values = oalSource->GetLooping();
                break;
            case AL_BUFFER:
                *values = oalSource->GetBuffer();
                break;
            case AL_BUFFERS_QUEUED:
                *values = oalSource->GetQLength();
                break;
            case AL_BUFFERS_PROCESSED:
                *values = oalSource->GetBuffersProcessed();
                break;
            case AL_REFERENCE_DISTANCE:
                *values = (ALint) oalSource->GetReferenceDistance();
                break;
            case AL_ROLLOFF_FACTOR:
                *values = (ALint) oalSource->GetRollOffFactor();
                break;
            case AL_MAX_DISTANCE:
                *values = (ALint) oalSource->GetMaxDistance();
                break;
            case AL_DIRECTION:
                oalSource->GetDirection((Float32&) values[0], (Float32&) values[1], (Float32&) values[2]);
                break;
            case AL_CONE_INNER_ANGLE:
                *values = (UInt32) oalSource->GetConeInnerAngle();
                break;
            case AL_CONE_OUTER_ANGLE:
                *values = (UInt32) oalSource->GetConeOuterAngle();
                break;
            case AL_SEC_OFFSET:
                *values = oalSource->GetQueueOffset(kSecondsOffset);
                break;
            case AL_SAMPLE_OFFSET:
                *values = oalSource->GetQueueOffset(kSampleOffset);
                break;
            case AL_BYTE_OFFSET:
                *values = oalSource->GetQueueOffset(kByteOffset);
                break;
            case AL_SOURCE_STATE:
                *values = oalSource->GetState();
                break;

			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->GetPosition((Float32&) values[0], (Float32&) values[1], (Float32&) values[2]);
                break;
            case AL_VELOCITY:
                oalSource->GetVelocity((Float32&) values[0], (Float32&) values[1], (Float32&) values[2]);
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alGetSourceiv FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alGetSourceiv FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetSource3i( ALuint sid, ALenum pname, ALint* v1, ALint*  v2, ALint*  v3)
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alGetSource3i--> source %ld : %s", (long int) sid, GetALAttributeString(pname));
#endif
	
	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch (pname) 
        {
			// Source ONLY Attributes
            case AL_DIRECTION:
                oalSource->GetDirection((Float32&) *v1, (Float32&)*v2, (Float32&)*v3);
                break;
				
			// Source & Listener Attributes
            case AL_POSITION:
                oalSource->GetPosition((Float32&) *v1, (Float32&)*v2, (Float32&)*v3);
                break;
            case AL_VELOCITY:
                oalSource->GetVelocity((Float32&) *v1, (Float32&)*v2, (Float32&)*v3);
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alGetSource3i FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alGetSource3i FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourcePlay(ALuint sid)
{	
#if LOG_SOURCE_USAGE
	DebugMessageN1("alSourcePlay--> source %ld", (long int) sid);
#endif

	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

		oalSource->Play();					// start playing the queue
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourcePlay FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourcePlay FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourcePause (ALuint sid)
{
#if LOG_SOURCE_USAGE
	DebugMessageN1("alSourcePause--> source %ld", (long int) sid);
#endif

	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

		oalSource->Pause();
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourcePause FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourcePause FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourceStop (ALuint sid)
{
#if LOG_SOURCE_USAGE
	DebugMessageN1("alSourceStop--> source %ld", (long int) sid);
#endif

	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        oalSource->Stop();
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourceStop FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourceStop FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourceRewind (ALuint sid)
{
#if LOG_SOURCE_USAGE
	DebugMessageN1("alSourceRewind--> source %ld", (long int) sid);
#endif

	OALSource *oalSource = NULL;
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        oalSource->Rewind();
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourceRewind FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourceRewind FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alSourcePlayv( ALsizei ns, const ALuint *sids )
{
#if LOG_SOURCE_USAGE
	DebugMessage("alSourcePlayv--> sources = ");
	for (UInt32	i = 0; i < (UInt32) ns; i++) {
		printf("%ld ", (long int) sids[i]);
	}
	printf("\n");
#endif

	try {
        for (UInt32	i = 0; i < (UInt32) ns; i++)
            alSourcePlay(sids[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourcePlayv FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourcePlayv FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alSourcePausev( ALsizei ns, const ALuint *sids )
{
#if LOG_SOURCE_USAGE
	DebugMessage("alSourcePausev--> sources = ");
	for (UInt32	i = 0; i < (UInt32) ns; i++) {
		printf("%ld ", (long int) sids[i]);
	}
	printf("\n");
#endif

	try {
        for (UInt32	i = 0; i < (UInt32) ns; i++)
            alSourcePause(sids[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourcePausev FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourcePausev FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alSourceStopv( ALsizei ns, const ALuint *sids )
{
#if LOG_SOURCE_USAGE
	DebugMessage("alSourceStopv--> sources = ");
	for (UInt32	i = 0; i < (UInt32) ns; i++) {
		printf("%ld ", (long int) sids[i]);
	}
	printf("\n");
#endif

	try {
        for (UInt32	i = 0; i < (UInt32) ns; i++)
            alSourceStop(sids[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourceStopv FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourceStopv FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alSourceRewindv(ALsizei ns, const ALuint *sids)
{
#if LOG_SOURCE_USAGE
	DebugMessage("alSourceRewindv--> sources = ");
	for (UInt32	i = 0; i < (UInt32) ns; i++) {
		printf("%ld ", (long int) sids[i]);
	}
	printf("\n");
#endif

	try {
        for (UInt32	i = 0; i < (UInt32) ns; i++)
            alSourceRewind(sids[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourceRewindv FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
    catch (...) {
		DebugMessage("ERROR: alSourceRewindv FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void	AL_APIENTRY alSourceQueueBuffers( ALuint sid, ALsizei numEntries, const ALuint* bids )
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alSourceQueueBuffers--> source %ld : numEntries = %ld", (long int) sid, (long int) numEntries);
#endif

#if LOG_BUFFER_USAGE
	printf("alSourceQueueBuffers--> (%ld) ", (long int) numEntries);
	for (UInt32	i = 0; i < (UInt32) numEntries; i++) {
		printf("%ld, ", (long int) bids[i]);
	}
	printf("\n");
#endif
	

	if (numEntries == 0)
		return;	// no buffers were actually requested for queueing

	OALSource		*oalSource = NULL;
		
	try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

		oalSource = ProtectSourceObjectInCurrentContext(sid);
                 
        //If the source is transitioning to flush Q, add the buffers to the temporary queue for now
        if(oalSource->IsSourceTransitioningToFlushQ())
        {
            // verify that buffers provided are valid before queueing them.
            for (UInt32	i = 0; i < (UInt32) numEntries; i++)
            {
                if (bids[i] != AL_NONE)
                {
                    // verify that this is a valid buffer
                    OALBuffer *oalBuffer = gOALBufferMap->Get(bids[i]);
                    if (oalBuffer == NULL)
                        throw ((OSStatus) AL_INVALID_VALUE); 
                }
            }
            
            // all valid buffers, so add them to the queue in Post Render
            for (UInt32	i = 0; i < (UInt32) numEntries; i++)
            {
                oalSource->AddToTempQueue(bids[i], gOALBufferMap->Get(bids[i]));
            }
        }
        else
        {
            // The source type must now be checked for static or streaming
            // It is illegal to append buffers to a Q, because a static designation means it only can use 1 buffer
            if (oalSource->GetSourceType() == AL_STATIC)
            {
                DebugMessage("ERROR: alSourceQueueBuffers FAILED oalSource->GetSourceType() == AL_STATIC");
                throw ((OSStatus) AL_INVALID_OPERATION);
            }
            
            CAGuard::Locker locked(*gBufferMapLock);
            
            // verify that buffers provided are valid before queueing them.
            for (UInt32	i = 0; i < (UInt32) numEntries; i++)
            {
                if (bids[i] != AL_NONE)
                {
                    // verify that this is a valid buffer
                    OALBuffer *oalBuffer = gOALBufferMap->Get(bids[i]);
                    if (oalBuffer == NULL)
                    {
                        DebugMessage("ERROR: alSourceQueueBuffers FAILED oalBuffer == NULL");
                        throw ((OSStatus) AL_INVALID_VALUE);				// an invalid buffer id has been provided
                    }
                }
            }
            
            // all valid buffers, so append them to the queue
            for (UInt32	i = 0; i < (UInt32) numEntries; i++)
            {
                oalSource->AddToQueue(bids[i], gOALBufferMap->Get(bids[i]));
            }
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alSourceQueueBuffers FAILED = %s\n", alGetString(result));
		alSetError(result);
	}
	catch (...) {
		DebugMessage("ERROR: alSourceQueueBuffers FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourceUnqueueBuffers (ALuint sid, ALsizei n, ALuint *buffers)
{
#if LOG_SOURCE_USAGE
	DebugMessageN2("alSourceUnqueueBuffers--> source %ld : count = %ld", (long int) sid, (long int) n);
#endif

	if (n == 0)
		return;

	OALSource		*oalSource = NULL;

	try {
        if (buffers == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);   

        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

		oalSource = ProtectSourceObjectInCurrentContext(sid);

		CAGuard::Locker locked(*gBufferMapLock);

        if (oalSource->GetQLength() < (UInt32) n)
            throw (OSStatus) AL_INVALID_VALUE;				// n is greater than the source's Q length
        
		oalSource->RemoveBuffersFromQueue(n, buffers);		
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alSourceUnqueueBuffers FAILED = %s\n", alGetString(result));
		// reinitialize the elements in the buffers array
		if (buffers)
		{
			for (UInt32	i = 0; i < (UInt32) n; i++)
				buffers[i] = 0;
		}
		// this would be real bad, as now we have a buffer queue in an unknown state
		alSetError(result);
	}
	catch (...){
		DebugMessage("ERROR: alSourceUnqueueBuffers FAILED");
        alSetError(AL_INVALID_OPERATION);
	}

	ReleaseSourceObject(oalSource);
	
#if LOG_BUFFER_USAGE
	printf("sid = %ld alSourceUnqueueBuffers--> (%ld) ", (long int) sid, (long int) n);
	for (UInt32	i = 0; i < (UInt32) n; i++) {
		printf("%ld, ", (long int) buffers[i]);
	}
	printf("\n");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Listeners *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alListenerf (ALenum pname, ALfloat value)
{
#if LOG_API_USAGE
	DebugMessageN2("alListenerf--> attribute = %s : value %.2f", GetALAttributeString(pname), value);
#endif
	OALContext		*oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (pname) 
        {
            case AL_GAIN:
				oalContext->SetListenerGain((Float32) value);     //gListener.Gain=value;
                break;
            
			default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alListenerf FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alListenerf FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alListenerfv (ALenum pname, const ALfloat *values)
{
#if LOG_API_USAGE
	DebugMessageN1("alListenerfv--> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext		*oalContext = NULL;
 
	try {
       
		oalContext = ProtectContextObject(gCurrentContext);
        switch(pname) 
        {
            case AL_POSITION:
                oalContext->SetListenerPosition(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalContext->SetListenerVelocity(values[0], values[1], values[2]);
                break;
            case AL_GAIN:
                oalContext->SetListenerGain((Float32) *values);
                break;
            case AL_ORIENTATION:
                oalContext->SetListenerOrientation(	values[0], values[1], values[2],
                                                    values[3], values[4], values[5]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alListenerfv FAILED = %s\n", alGetString(result));
       alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alListenerfv FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alListener3f (ALenum pname, ALfloat v1, ALfloat v2, ALfloat v3)
{
#if LOG_API_USAGE
	DebugMessageN4("alListener3f--> attribute = %s : %.2f : %.2f : %.2f", GetALAttributeString(pname), v1, v2, v3);
#endif

	OALContext		*oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (pname) 
        {
            case AL_POSITION:
                oalContext->SetListenerPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalContext->SetListenerVelocity(v1, v2, v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alListener3f FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
		DebugMessage("ERROR: alListener3f FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
		
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alListeneri (ALenum pname, ALint value)
{		
#if LOG_API_USAGE
	DebugMessage("***** alListeneri");
#endif
	alSetError(AL_INVALID_ENUM);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alListeneriv( ALenum pname, const ALint* values )
{
#if LOG_API_USAGE
	DebugMessageN1("alListeneriv--> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext		*oalContext = NULL;

	try {
		oalContext = ProtectContextObject(gCurrentContext);
        switch(pname) 
        {
            case AL_POSITION:
                oalContext->SetListenerPosition(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalContext->SetListenerVelocity(values[0], values[1], values[2]);
                break;
            case AL_ORIENTATION:
                oalContext->SetListenerOrientation(	values[0], values[1], values[2],
                                                    values[3], values[4], values[5]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alListeneriv FAILED = %s\n", alGetString(result));
       alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alListeneriv FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alListener3i( ALenum pname, ALint v1, ALint v2, ALint v3 )
{
#if LOG_API_USAGE
	DebugMessageN4("alListener3i--> attribute = %s : %ld : %ld : %ld", GetALAttributeString(pname), (long int) v1, (long int) v2, (long int) v3);
#endif

	OALContext		*oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (pname) 
        {
            case AL_POSITION:
                oalContext->SetListenerPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalContext->SetListenerVelocity(v1, v2, v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alListener3f FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
		DebugMessage("ERROR: alListener3f FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetListenerf( ALenum pname, ALfloat* value )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetListenerf--> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext* oalContext = NULL;
	
	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch(pname) 
        {
            case AL_GAIN:
                *value = oalContext->GetListenerGain();
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alGetListenerf FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
		DebugMessage("ERROR: alGetListenerf FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetListenerfv( ALenum pname, ALfloat* values )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetListenerfv--> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext* oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (pname) 
        {
            case AL_POSITION:
                oalContext->GetListenerPosition(&values[0], &values[1], &values[2]);
                break;
            case AL_VELOCITY:
                oalContext->GetListenerVelocity(&values[0], &values[1], &values[2]);
                break;
            case AL_GAIN:
                *values = oalContext->GetListenerGain();
                break;
            case AL_ORIENTATION:
                oalContext->GetListenerOrientation( &values[0], &values[1], &values[2],
                                                    &values[3], &values[4], &values[5]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alGetListenerfv FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
		DebugMessage("ERROR: alGetListenerfv FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}	

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetListener3f( ALenum pname, ALfloat* v1, ALfloat* v2, ALfloat* v3 )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetListener3f--> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext* oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch(pname) 
        {
            case AL_POSITION:
                oalContext->GetListenerPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalContext->GetListenerVelocity(v1, v2, v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alGetListener3f FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
		DebugMessage("ERROR: alGetListener3f FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetListeneri( ALenum pname, ALint* value )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetListeneri--> attribute = %s", GetALAttributeString(pname));
#endif

	*value = 0;

	try {
		switch (pname)
		{
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (...) {
 		DebugMessage("ERROR: alGetListeneri FAILED");
        alSetError(AL_INVALID_OPERATION); // not available yet as the device is not setup
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetListeneriv( ALenum pname, ALint* values )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetListeneriv--> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext* oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (pname) 
        {
            case AL_POSITION:
                oalContext->GetListenerPosition((Float32*) &values[0], (Float32*) &values[1], (Float32*) &values[2]);
                break;
            case AL_VELOCITY:
                oalContext->GetListenerVelocity((Float32*) &values[0], (Float32*) &values[1], (Float32*) &values[2]);
                break;
            case AL_ORIENTATION:
                oalContext->GetListenerOrientation( (Float32*) &values[0], (Float32*) &values[1], (Float32*) &values[2],
                                                    (Float32*) &values[3], (Float32*) &values[4], (Float32*) &values[5]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alGetListenerfv FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
		DebugMessage("ERROR: alGetListenerfv FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alGetListener3i( ALenum pname, ALint *v1, ALint *v2, ALint *v3 )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetListener3i--> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext* oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch(pname) 
        {
            case AL_POSITION:
                oalContext->GetListenerPosition((Float32*) v1, (Float32*) v2, (Float32*) v3);
                break;
            case AL_VELOCITY:
                oalContext->GetListenerVelocity((Float32*) v1, (Float32*) v2, (Float32*) v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alGetListener3f FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
		DebugMessage("ERROR: alGetListener3f FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Global Settings *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY	alDistanceModel (ALenum value)
{
#if LOG_API_USAGE
	DebugMessageN1("alDistanceModel--> model = %s", GetALAttributeString(value));
#endif

	OALContext* oalContext = NULL;

	try {
        switch (value)
        {
            case AL_NONE:			
            case AL_INVERSE_DISTANCE:
            case AL_INVERSE_DISTANCE_CLAMPED:
            case AL_LINEAR_DISTANCE:
            case AL_LINEAR_DISTANCE_CLAMPED:
            case AL_EXPONENT_DISTANCE:
            case AL_EXPONENT_DISTANCE_CLAMPED:
            {
				oalContext = ProtectContextObject(gCurrentContext);
				oalContext->SetDistanceModel(value);
            } 
            break;
            
            default:
                alSetError(AL_INVALID_VALUE);
                break;
        }
    }
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alDistanceModel FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessage("ERROR: alDistanceModel FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
    }

	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alDopplerFactor (ALfloat value)
{	
#if LOG_API_USAGE
	DebugMessageN1("alDopplerFactor---> setting = %.f2", value);
#endif

	OALContext* oalContext = NULL;

	try {
        if (value < 0.0f)
            throw ((OSStatus) AL_INVALID_VALUE);

        oalContext = ProtectContextObject(gCurrentContext);
		oalContext->SetDopplerFactor(value);
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alDopplerFactor FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessage("ERROR: alDopplerFactor FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
    }
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alDopplerVelocity (ALfloat value)
{
#if LOG_API_USAGE
	DebugMessageN1("alDopplerVelocity---> setting = %.f2", value);
#endif

	OALContext* oalContext = NULL;

	try {
        if (value <= 0.0f)
            throw ((OSStatus) AL_INVALID_VALUE);

        oalContext = ProtectContextObject(gCurrentContext);
		oalContext->SetDopplerVelocity(value);
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alDopplerVelocity FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessage("ERROR: alDopplerVelocity FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
    }
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void AL_APIENTRY alSpeedOfSound( ALfloat value )
{
#if LOG_API_USAGE
	DebugMessageN1("alSpeedOfSound---> setting = %.f2", value);
#endif

	OALContext* oalContext = NULL;

	try {
		if (value <= 0.0f)
			throw ((OSStatus) AL_INVALID_VALUE);

        oalContext = ProtectContextObject(gCurrentContext);
		oalContext->SetSpeedOfSound(value);
	}
	catch (OSStatus		result) {
		DebugMessageN1("ERROR: alSpeedOfSound FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessage("ERROR: alSpeedOfSound FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
    }
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API const ALchar* AL_APIENTRY alGetString( ALenum pname )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetString = %s", GetALAttributeString(pname));
#endif

	switch (pname)
	{
		case AL_VENDOR:
			return (ALchar *)alVendor;
		case AL_VERSION:
			return (ALchar *)alVersion;
		case AL_RENDERER:
			return (ALchar *)alRenderer;
		case AL_EXTENSIONS:
			return (ALchar *)GetALExtensionList();
		case AL_NO_ERROR:
			return (ALchar *)alNoError;
		case AL_INVALID_NAME:
			return (ALchar *)alErrInvalidName;
		case AL_INVALID_ENUM:
			return (ALchar *)alErrInvalidEnum;
		case AL_INVALID_VALUE:
			return (ALchar *)alErrInvalidValue;
		case AL_INVALID_OPERATION:
			return (ALchar *)alErrInvalidOp;
		case AL_OUT_OF_MEMORY:
			return (ALchar *)alErrOutOfMemory;
		case -1:
			return (ALchar *)unknownImplementationError;
		default:
			alSetError(AL_INVALID_ENUM);
			break;
	}
	return NULL;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALenum	AL_APIENTRY alGetEnumValue( const ALchar* ename )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetEnumValue: %s", ename);
#endif

	// AL
	if (strcmp("AL_INVALID", (const char *)ename) == 0) { return AL_INVALID; }
	if (strcmp("AL_NONE", (const char *)ename) == 0) { return AL_NONE; }
	if (strcmp("AL_FALSE", (const char *)ename) == 0) { return AL_FALSE; }
	if (strcmp("AL_TRUE", (const char *)ename) == 0) { return AL_TRUE; }
	if (strcmp("AL_SOURCE_RELATIVE", (const char *)ename) == 0) { return AL_SOURCE_RELATIVE; }
	if (strcmp("AL_CONE_INNER_ANGLE", (const char *)ename) == 0) { return AL_CONE_INNER_ANGLE; }
	if (strcmp("AL_CONE_OUTER_ANGLE", (const char *)ename) == 0) { return AL_CONE_OUTER_ANGLE; }
	if (strcmp("AL_CONE_OUTER_GAIN", (const char *)ename) == 0) { return AL_CONE_OUTER_GAIN; }
	if (strcmp("AL_PITCH", (const char *)ename) == 0) { return AL_PITCH; }
	if (strcmp("AL_POSITION", (const char *)ename) == 0) { return AL_POSITION; }
	if (strcmp("AL_DIRECTION", (const char *)ename) == 0) { return AL_DIRECTION; }
	if (strcmp("AL_VELOCITY", (const char *)ename) == 0) { return AL_VELOCITY; }
	if (strcmp("AL_LOOPING", (const char *)ename) == 0) { return AL_LOOPING; }
	if (strcmp("AL_BUFFER", (const char *)ename) == 0) { return AL_BUFFER; }
	if (strcmp("AL_GAIN", (const char *)ename) == 0) { return AL_GAIN; }
	if (strcmp("AL_MIN_GAIN", (const char *)ename) == 0) { return AL_MIN_GAIN; }
	if (strcmp("AL_MAX_GAIN", (const char *)ename) == 0) { return AL_MAX_GAIN; }
	if (strcmp("AL_ORIENTATION", (const char *)ename) == 0) { return AL_ORIENTATION; }
	if (strcmp("AL_REFERENCE_DISTANCE", (const char *)ename) == 0) { return AL_REFERENCE_DISTANCE; }
	if (strcmp("AL_ROLLOFF_FACTOR", (const char *)ename) == 0) { return AL_ROLLOFF_FACTOR; }
	if (strcmp("AL_MAX_DISTANCE", (const char *)ename) == 0) { return AL_MAX_DISTANCE; }
	if (strcmp("AL_SOURCE_STATE", (const char *)ename) == 0) { return AL_SOURCE_STATE; }
	if (strcmp("AL_INITIAL", (const char *)ename) == 0) { return AL_INITIAL; }
	if (strcmp("AL_PLAYING", (const char *)ename) == 0) { return AL_PLAYING; }
	if (strcmp("AL_PAUSED", (const char *)ename) == 0) { return AL_PAUSED; }
	if (strcmp("AL_STOPPED", (const char *)ename) == 0) { return AL_STOPPED; }
	if (strcmp("AL_BUFFERS_QUEUED", (const char *)ename) == 0) { return AL_BUFFERS_QUEUED; }
	if (strcmp("AL_BUFFERS_PROCESSED", (const char *)ename) == 0) { return AL_BUFFERS_PROCESSED; }
	if (strcmp("AL_FORMAT_MONO8", (const char *)ename) == 0) { return AL_FORMAT_MONO8; }
	if (strcmp("AL_FORMAT_MONO16", (const char *)ename) == 0) { return AL_FORMAT_MONO16; }
	if (strcmp("AL_FORMAT_STEREO8", (const char *)ename) == 0) { return AL_FORMAT_STEREO8; }
	if (strcmp("AL_FORMAT_STEREO16", (const char *)ename) == 0) { return AL_FORMAT_STEREO16; }
	if (strcmp("AL_FREQUENCY", (const char *)ename) == 0) { return AL_FREQUENCY; }
	if (strcmp("AL_SIZE", (const char *)ename) == 0) { return AL_SIZE; }
	if (strcmp("AL_UNUSED", (const char *)ename) == 0) { return AL_UNUSED; }
	if (strcmp("AL_PENDING", (const char *)ename) == 0) { return AL_PENDING; }
	if (strcmp("AL_PROCESSED", (const char *)ename) == 0) { return AL_PROCESSED; }
	if (strcmp("AL_NO_ERROR", (const char *)ename) == 0) { return AL_NO_ERROR; }
	if (strcmp("AL_INVALID_NAME", (const char *)ename) == 0) { return AL_INVALID_NAME; }
	if (strcmp("AL_INVALID_ENUM", (const char *)ename) == 0) { return AL_INVALID_ENUM; }
	if (strcmp("AL_INVALID_VALUE", (const char *)ename) == 0) { return AL_INVALID_VALUE; }
	if (strcmp("AL_INVALID_OPERATION", (const char *)ename) == 0) { return AL_INVALID_OPERATION; }
	if (strcmp("AL_OUT_OF_MEMORY", (const char *)ename) == 0) { return AL_OUT_OF_MEMORY; }
	if (strcmp("AL_VENDOR", (const char *)ename) == 0) { return AL_VENDOR; }
	if (strcmp("AL_VERSION", (const char *)ename) == 0) { return AL_VERSION; }
	if (strcmp("AL_RENDERER", (const char *)ename) == 0) { return AL_RENDERER; }
	if (strcmp("AL_EXTENSIONS", (const char *)ename) == 0) { return AL_EXTENSIONS; }
	if (strcmp("AL_DOPPLER_FACTOR", (const char *)ename) == 0) { return AL_DOPPLER_FACTOR; }
	if (strcmp("AL_DOPPLER_VELOCITY", (const char *)ename) == 0) { return AL_DOPPLER_VELOCITY; }
	if (strcmp("AL_DISTANCE_MODEL", (const char *)ename) == 0) { return AL_DISTANCE_MODEL; }
	if (strcmp("AL_INVERSE_DISTANCE", (const char *)ename) == 0) { return AL_INVERSE_DISTANCE; }
	if (strcmp("AL_INVERSE_DISTANCE_CLAMPED", (const char *)ename) == 0) { return AL_INVERSE_DISTANCE_CLAMPED; }
	if (strcmp("AL_LINEAR_DISTANCE", (const char *)ename) == 0) { return AL_LINEAR_DISTANCE; }
	if (strcmp("AL_LINEAR_DISTANCE_CLAMPED", (const char *)ename) == 0) { return AL_LINEAR_DISTANCE_CLAMPED; }
	if (strcmp("AL_EXPONENT_DISTANCE", (const char *)ename) == 0) { return AL_EXPONENT_DISTANCE; }
	if (strcmp("AL_EXPONENT_DISTANCE_CLAMPED", (const char *)ename) == 0) { return AL_EXPONENT_DISTANCE_CLAMPED; }
	if (strcmp("AL_SPEED_OF_SOUND", (const char *)ename) == 0) { return AL_SPEED_OF_SOUND; }
	if (strcmp("AL_SOURCE_TYPE", (const char *)ename) == 0) { return AL_SOURCE_TYPE; }
	// ALC
	if (strcmp("ALC_INVALID", (const char *)ename) == 0) { return ALC_INVALID; }
	if (strcmp("ALC_FALSE", (const char *)ename) == 0) { return ALC_FALSE; }
	if (strcmp("ALC_TRUE", (const char *)ename) == 0) { return ALC_TRUE; }
	if (strcmp("ALC_MAJOR_VERSION", (const char *)ename) == 0) { return ALC_MAJOR_VERSION; }
	if (strcmp("ALC_MINOR_VERSION", (const char *)ename) == 0) { return ALC_MINOR_VERSION; }
	if (strcmp("ALC_ATTRIBUTES_SIZE", (const char *)ename) == 0) { return ALC_ATTRIBUTES_SIZE; }
	if (strcmp("ALC_ALL_ATTRIBUTES", (const char *)ename) == 0) { return ALC_ALL_ATTRIBUTES; }
	if (strcmp("ALC_DEFAULT_DEVICE_SPECIFIER", (const char *)ename) == 0) { return ALC_DEFAULT_DEVICE_SPECIFIER; }
	if (strcmp("ALC_DEVICE_SPECIFIER", (const char *)ename) == 0) { return ALC_DEVICE_SPECIFIER; }
	if (strcmp("ALC_EXTENSIONS", (const char *)ename) == 0) { return ALC_EXTENSIONS; }
	if (strcmp("ALC_FREQUENCY", (const char *)ename) == 0) { return ALC_FREQUENCY; }
	if (strcmp("ALC_REFRESH", (const char *)ename) == 0) { return ALC_REFRESH; }
	if (strcmp("ALC_SYNC", (const char *)ename) == 0) { return ALC_SYNC; }
	if (strcmp("ALC_NO_ERROR", (const char *)ename) == 0) { return ALC_NO_ERROR; }
	if (strcmp("ALC_INVALID_DEVICE", (const char *)ename) == 0) { return ALC_INVALID_DEVICE; }
	if (strcmp("ALC_INVALID_CONTEXT", (const char *)ename) == 0) { return ALC_INVALID_CONTEXT; }
	if (strcmp("ALC_INVALID_ENUM", (const char *)ename) == 0) { return ALC_INVALID_ENUM; }
	if (strcmp("ALC_INVALID_VALUE", (const char *)ename) == 0) { return ALC_INVALID_VALUE; }
	if (strcmp("ALC_OUT_OF_MEMORY", (const char *)ename) == 0) { return ALC_OUT_OF_MEMORY; }
	if (strcmp("ALC_MONO_SOURCES", (const char *)ename) == 0) { return ALC_MONO_SOURCES; }
	if (strcmp("ALC_STEREO_SOURCES", (const char *)ename) == 0) { return ALC_STEREO_SOURCES; }
	// AL_EXT_OFFSET
	if (strcmp("AL_SEC_OFFSET", (const char *)ename) == 0) { return AL_SEC_OFFSET; }
	if (strcmp("AL_SAMPLE_OFFSET", (const char *)ename) == 0) { return AL_SAMPLE_OFFSET; }
	if (strcmp("AL_BYTE_OFFSET", (const char *)ename) == 0) { return AL_BYTE_OFFSET; }
	// ALC_EXT_capture
	if (strcmp("ALC_CAPTURE_DEVICE_SPECIFIER", (const char *)ename) == 0) { return ALC_CAPTURE_DEVICE_SPECIFIER; }
	if (strcmp("ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER", (const char *)ename) == 0) { return ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER; }
	if (strcmp("ALC_CAPTURE_SAMPLES", (const char *)ename) == 0) { return ALC_CAPTURE_SAMPLES; }
	// ALC_EXT_float32
	if (strcmp("AL_FORMAT_MONO_FLOAT32", (const char *)ename) == 0) { return AL_FORMAT_MONO_FLOAT32; }
	if (strcmp("AL_FORMAT_STEREO_FLOAT32", (const char *)ename) == 0) { return AL_FORMAT_STEREO_FLOAT32; }
	// ALC_EXT_MAC_OSX
	// 1.0 implementation names (remains for backward compatibility)
	if (strcmp("ALC_SPATIAL_RENDERING_QUALITY", (const char *)ename) == 0) { return ALC_SPATIAL_RENDERING_QUALITY; }
	if (strcmp("ALC_MIXER_OUTPUT_RATE", (const char *)ename) == 0) { return ALC_MIXER_OUTPUT_RATE; }
	if (strcmp("ALC_MIXER_MAXIMUM_BUSSES", (const char *)ename) == 0) { return ALC_MIXER_MAXIMUM_BUSSES; }
	if (strcmp("ALC_RENDER_CHANNEL_COUNT", (const char *)ename) == 0) { return ALC_RENDER_CHANNEL_COUNT; }
	
	if (strcmp("ALC_CONVERT_DATA_UPON_LOADING", (const char *)ename) == 0) { return ALC_MAC_OSX_CONVERT_DATA_UPON_LOADING; }	
	if (strcmp("ALC_SPATIAL_RENDERING_QUALITY_HIGH", (const char *)ename) == 0) { return ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_HIGH; }
	if (strcmp("ALC_SPATIAL_RENDERING_QUALITY_LOW", (const char *)ename) == 0) { return ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_LOW; }
	if (strcmp("ALC_RENDER_CHANNEL_COUNT_STEREO", (const char *)ename) == 0) { return ALC_MAC_OSX_RENDER_CHANNEL_COUNT_STEREO; }
	if (strcmp("ALC_RENDER_CHANNEL_COUNT_MULTICHANNEL", (const char *)ename) == 0) { return ALC_MAC_OSX_RENDER_CHANNEL_COUNT_MULTICHANNEL; }
	// 1.1 implementation names
	if (strcmp("ALC_MAC_OSX_CONVERT_DATA_UPON_LOADING", (const char *)ename) == 0) { return ALC_MAC_OSX_CONVERT_DATA_UPON_LOADING; }
	if (strcmp("ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_HIGH", (const char *)ename) == 0) { return ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_HIGH; }
	if (strcmp("ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_LOW", (const char *)ename) == 0) { return ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_LOW; }
	if (strcmp("ALC_MAC_OSX_RENDER_CHANNEL_COUNT_STEREO", (const char *)ename) == 0) { return ALC_MAC_OSX_RENDER_CHANNEL_COUNT_STEREO; }
	if (strcmp("ALC_MAC_OSX_RENDER_CHANNEL_COUNT_MULTICHANNEL", (const char *)ename) == 0) { return ALC_MAC_OSX_RENDER_CHANNEL_COUNT_MULTICHANNEL; }
	
	if (strcmp("ALC_ASA_REVERB_ON", (const char *)ename) == 0) { return ALC_ASA_REVERB_ON; }
	if (strcmp("ALC_ASA_REVERB_EQ_GAIN", (const char *)ename) == 0) { return ALC_ASA_REVERB_EQ_GAIN; }
	if (strcmp("ALC_ASA_REVERB_EQ_BANDWITH", (const char *)ename) == 0) { return ALC_ASA_REVERB_EQ_BANDWITH; }
	if (strcmp("ALC_ASA_REVERB_EQ_FREQ", (const char *)ename) == 0) { return ALC_ASA_REVERB_EQ_FREQ; }
	if (strcmp("ALC_ASA_REVERB_PRESET", (const char *)ename) == 0) { return ALC_ASA_REVERB_PRESET; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE; }
	if (strcmp("ALC_ASA_REVERB_SEND_LEVEL", (const char *)ename) == 0) { return ALC_ASA_REVERB_SEND_LEVEL; }
	if (strcmp("ALC_ASA_REVERB_GLOBAL_LEVEL", (const char *)ename) == 0) { return ALC_ASA_REVERB_GLOBAL_LEVEL; }
	if (strcmp("ALC_ASA_REVERB_QUALITY", (const char *)ename) == 0) { return ALC_ASA_REVERB_QUALITY; }
	if (strcmp("ALC_ASA_OCCLUSION", (const char *)ename) == 0) { return ALC_ASA_OCCLUSION; }
	if (strcmp("ALC_ASA_OBSTRUCTION", (const char *)ename) == 0) { return ALC_ASA_OBSTRUCTION; }

	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_SmallRoom", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_SmallRoom; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_MediumRoom", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_MediumRoom; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_LargeRoom", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_LargeRoom; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_MediumHall", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_MediumHall; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_LargeHall", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_LargeHall; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_Plate", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_Plate; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_MediumChamber", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_MediumChamber; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_LargeChamber", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_LargeChamber; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_Cathedral", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_Cathedral; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_LargeRoom2", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_LargeRoom2; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_MediumHall2", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_MediumHall2; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_MediumHall3", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_MediumHall3; }
	if (strcmp("ALC_ASA_REVERB_ROOM_TYPE_LargeHall2", (const char *)ename) == 0) { return ALC_ASA_REVERB_ROOM_TYPE_LargeHall2; }

	if (strcmp("ALC_ASA_REVERB_QUALITY_Max", (const char *)ename) == 0) { return ALC_ASA_REVERB_QUALITY_Max; }
	if (strcmp("ALC_ASA_REVERB_QUALITY_High", (const char *)ename) == 0) { return ALC_ASA_REVERB_QUALITY_High; }
	if (strcmp("ALC_ASA_REVERB_QUALITY_Medium", (const char *)ename) == 0) { return ALC_ASA_REVERB_QUALITY_Medium; }
	if (strcmp("ALC_ASA_REVERB_QUALITY_Low", (const char *)ename) == 0) { return ALC_ASA_REVERB_QUALITY_Low; }
	if (strcmp("ALC_ASA_REVERB_QUALITY_Min", (const char *)ename) == 0) { return ALC_ASA_REVERB_QUALITY_Min; }

	if (strcmp("ALC_ASA_ROGER_BEEP_ENABLE", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_ENABLE; }
	if (strcmp("ALC_ASA_ROGER_BEEP_ON", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_ON; }
	if (strcmp("ALC_ASA_ROGER_BEEP_GAIN", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_GAIN; }
	if (strcmp("ALC_ASA_ROGER_BEEP_SENSITIVITY", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_SENSITIVITY; }
	if (strcmp("ALC_ASA_ROGER_BEEP_TYPE", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_TYPE; }
	if (strcmp("ALC_ASA_ROGER_BEEP_PRESET", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_PRESET; }

	if (strcmp("ALC_ASA_ROGER_BEEP_TYPE_quindartone", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_TYPE_quindartone; }
	if (strcmp("ALC_ASA_ROGER_BEEP_TYPE_whitenoise", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_TYPE_whitenoise; }
	if (strcmp("ALC_ASA_ROGER_BEEP_TYPE_walkietalkie", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_TYPE_walkietalkie; }
	if (strcmp("ALC_ASA_ROGER_BEEP_SENSITIVITY_Light", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_SENSITIVITY_Light; }
	if (strcmp("ALC_ASA_ROGER_BEEP_SENSITIVITY_Medium", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_SENSITIVITY_Medium; }
	if (strcmp("ALC_ASA_ROGER_BEEP_SENSITIVITY_Heavy", (const char *)ename) == 0) { return ALC_ASA_ROGER_BEEP_SENSITIVITY_Heavy; }

	if (strcmp("ALC_ASA_DISTORTION_ENABLE", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_ENABLE; }
	if (strcmp("ALC_ASA_DISTORTION_ON", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_ON; }
	if (strcmp("ALC_ASA_DISTORTION_MIX", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_MIX; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE; }
	if (strcmp("ALC_ASA_DISTORTION_PRESET", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_PRESET; }

	if (strcmp("ALC_ASA_DISTORTION_TYPE_BitBrush", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_BitBrush; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_BufferBeats", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_BufferBeats; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_LoFi", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_LoFi; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_BitBrush", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_BitBrush; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_BrokenSpeaker", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_BrokenSpeaker; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Cellphone", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Cellphone; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Decimated1", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Decimated1; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Decimated2", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Decimated2; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Decimated3", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Decimated3; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Decimated4", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Decimated4; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_DistortedFunk", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_DistortedFunk; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_DistortionCubed", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_DistortionCubed; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_DistortionSquared", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_DistortionSquared; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Echo1", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Echo1; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Echo2", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Echo2; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_EchoTight1", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_EchoTight1; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_EchoTight2", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_EchoTight2; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_EverythingBroken", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_EverythingBroken; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_AlienChatter", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_AlienChatter; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_CosmicInteference", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_CosmicInteference; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_GoldenPi", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_GoldenPi; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_RadioTower", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_RadioTower; }
	if (strcmp("ALC_ASA_DISTORTION_TYPE_Waves", (const char *)ename) == 0) { return ALC_ASA_DISTORTION_TYPE_Waves; }

	return -1;
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALboolean AL_APIENTRY alGetBoolean (ALenum pname)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetBoolean");
#endif
	return AL_FALSE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetBooleanv (ALenum pname, ALboolean *data)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetBooleanv");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALfloat AL_APIENTRY alGetFloat (ALenum pname)
{
	Float32			returnValue = 0.0f;

#if LOG_API_USAGE
	DebugMessageN1("alGetFloat ---> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext* oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (pname)
        {
            case AL_SPEED_OF_SOUND:
                returnValue = oalContext->GetSpeedOfSound();
                break;
            case AL_DOPPLER_FACTOR:
                returnValue = oalContext->GetDopplerFactor();
                break;
            case AL_DOPPLER_VELOCITY:
                returnValue = oalContext->GetDopplerVelocity();
                break;
            default:
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alGetFloat FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alGetFloat FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
	
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetFloatv (ALenum pname, ALfloat *data)
{
#if LOG_API_USAGE
	DebugMessageN1("alGetFloatv ---> attribute = %s", GetALAttributeString(pname));
#endif

	OALContext* oalContext = NULL;

	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch(pname)
        {
            case AL_SPEED_OF_SOUND:
                *data = oalContext->GetSpeedOfSound();
                break;
            case AL_DOPPLER_FACTOR:
                *data = oalContext->GetDopplerFactor();
                break;
            case AL_DOPPLER_VELOCITY:
                *data = oalContext->GetDopplerVelocity();
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alGetFloatv FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alGetFloatv FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALdouble AL_APIENTRY alGetDouble (ALenum pname)
{
#if LOG_API_USAGE
	DebugMessageN1("alGetDouble ---> attribute = %s", GetALCAttributeString(pname));
#endif

    double      returnValue = 0.0;

	try {
		switch (pname)
		{			
			case ALC_MIXER_OUTPUT_RATE:
				returnValue = GetMixerOutputRate();
				break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alGetDouble FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alGetDouble FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return returnValue;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetDoublev (ALenum pname, ALdouble *data)
{
#if LOG_API_USAGE
	DebugMessageN1("alGetDoublev ---> attribute = %s", GetALCAttributeString(pname));
#endif

	try {
		switch (pname)
		{			
			case ALC_MIXER_OUTPUT_RATE:
				*data = GetMixerOutputRate();
				break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alGetDoublev FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alGetDoublev FAILED");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALint AL_APIENTRY alGetInteger (ALenum pname)
{
#if LOG_API_USAGE
	DebugMessageN1("alGetInteger ---> attribute = 0x%X", pname);
#endif

	UInt32			returnValue	= 0;
	OALContext		*oalContext = NULL;
	OALDevice		*oalDevice = NULL;
	
	try {
		switch (pname)
		{
            case AL_DISTANCE_MODEL:
				oalContext = ProtectContextObject(gCurrentContext);
				returnValue = oalContext->GetDistanceModel();
                break;

			case ALC_SPATIAL_RENDERING_QUALITY:
				oalContext = ProtectContextObject(gCurrentContext);
				returnValue = oalContext->GetRenderQuality();
				break;

			case ALC_RENDER_CHANNEL_COUNT:
			{
				oalDevice = ProtectDeviceObject (gCurrentDevice);
				returnValue = oalDevice->GetRenderChannelSetting();
			}
				break;
                
            case ALC_MIXER_MAXIMUM_BUSSES:
				oalContext = ProtectContextObject(gCurrentContext);
				if (oalContext)
					returnValue = oalContext->GetBusCount();
				else
					returnValue = gMaximumMixerBusCount;
                break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alGetInteger FAILED = %s\n", alGetString(result));
    }
	catch (...) {
 		DebugMessage("ERROR: alGetInteger FAILED");
	}
	
	ReleaseContextObject(oalContext);
	ReleaseDeviceObject(oalDevice);
	
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alGetIntegerv (ALenum pname, ALint *data)
{
#if LOG_API_USAGE
	DebugMessageN1("alGetIntegerv ---> attribute = 0x%X", pname);
#endif

	OALContext* oalContext	= NULL;
	OALDevice*	oalDevice	= NULL;
		
	try {
		oalContext = NULL;
        switch (pname)
        {
            case AL_DISTANCE_MODEL:
				oalContext = ProtectContextObject(gCurrentContext);
				*data = oalContext->GetDistanceModel();
                break;
                
			case ALC_SPATIAL_RENDERING_QUALITY:
				oalContext = ProtectContextObject(gCurrentContext);
				*data = oalContext->GetRenderQuality();
				break;

			case ALC_RENDER_CHANNEL_COUNT:
			{
				oalDevice = ProtectDeviceObject (gCurrentDevice);
				*data = oalDevice->GetRenderChannelSetting();
			}
				break;
                
            case ALC_MIXER_MAXIMUM_BUSSES:
				oalContext = ProtectContextObject(gCurrentContext);
				if (oalContext)
					*data = oalContext->GetBusCount();
				else
					*data = gMaximumMixerBusCount;
                break;

            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN1("ERROR: alGetIntegerv FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessage("ERROR: alGetIntegerv FAILED");
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseContextObject(oalContext);
	ReleaseDeviceObject(oalDevice);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API void*	AL_APIENTRY alGetProcAddress( const ALchar* fname )
{
#if LOG_API_USAGE
	DebugMessageN1("alGetProcAddress function name = %s", fname);
#endif

	if (fname == NULL)
		SetDeviceError(gCurrentDevice, ALC_INVALID_VALUE);
	else
	{
		// core APIs
		if (strcmp("alcOpenDevice", (const char *)fname) == 0) { return (void*) alcOpenDevice; }
		if (strcmp("alcCloseDevice", (const char *)fname) == 0) { return (void*) alcCloseDevice; }
		if (strcmp("alcGetError", (const char *)fname) == 0) { return (void*) alcGetError; }
		if (strcmp("alcCreateContext", (const char *)fname) == 0) { return (void*) alcCreateContext; }
		if (strcmp("alcMakeContextCurrent", (const char *)fname) == 0) { return (void*) alcMakeContextCurrent; }
		if (strcmp("alcProcessContext", (const char *)fname) == 0) { return (void*) alcProcessContext; }
		if (strcmp("alcGetCurrentContext", (const char *)fname) == 0) { return (void*) alcGetCurrentContext; }
		if (strcmp("alcGetContextsDevice", (const char *)fname) == 0) { return (void*) alcGetContextsDevice; }
		if (strcmp("alcSuspendContext", (const char *)fname) == 0) { return (void*) alcSuspendContext; }
		if (strcmp("alcDestroyContext", (const char *)fname) == 0) { return (void*)alcDestroyContext; }
		if (strcmp("alcGetString", (const char *)fname) == 0) { return (void*) alcGetString; }
		if (strcmp("alcIsExtensionPresent", (const char *)fname) == 0) { return (void*) alcIsExtensionPresent; }
		if (strcmp("alcGetProcAddress", (const char *)fname) == 0) { return (void*) alcGetProcAddress; }
		if (strcmp("alcGetEnumValue", (const char *)fname) == 0) { return (void*) alcGetEnumValue; }
		if (strcmp("alcGetInteger", (const char *)fname) == 0) { return (void*) alcGetInteger; }
		if (strcmp("alcGetIntegerv", (const char *)fname) == 0) { return (void*) alcGetIntegerv; }
		if (strcmp("alGetError", (const char *)fname) == 0) { return (void*) alGetError; }
		if (strcmp("alGenBuffers", (const char *)fname) == 0) { return (void*) alGenBuffers; }
		if (strcmp("alDeleteBuffers", (const char *)fname) == 0) { return (void*) alDeleteBuffers; }
		if (strcmp("alIsBuffer", (const char *)fname) == 0) { return (void*) alIsBuffer; }
		if (strcmp("alBufferData", (const char *)fname) == 0) { return (void*) alBufferData; }
		if (strcmp("alBufferf", (const char *)fname) == 0) { return (void*) alBufferf; }
		if (strcmp("alBuffer3f", (const char *)fname) == 0) { return (void*) alBuffer3f; }
		if (strcmp("alBufferfv", (const char *)fname) == 0) { return (void*) alBufferfv; }
		if (strcmp("alBufferi", (const char *)fname) == 0) { return (void*) alBufferi; }
		if (strcmp("alBuffer3i", (const char *)fname) == 0) { return (void*) alBuffer3i; }
		if (strcmp("alBufferiv", (const char *)fname) == 0) { return (void*) alBufferiv; }
		if (strcmp("alGetBufferf", (const char *)fname) == 0) { return (void*) alGetBufferf; }
		if (strcmp("alGetBuffer3f", (const char *)fname) == 0) { return (void*) alGetBuffer3f; }
		if (strcmp("alGetBufferfv", (const char *)fname) == 0) { return (void*) alGetBufferfv; }
		if (strcmp("alGetBufferi", (const char *)fname) == 0) { return (void*) alGetBufferi; }
		if (strcmp("alGetBuffer3i", (const char *)fname) == 0) { return (void*) alGetBuffer3i; }
		if (strcmp("alGetBufferiv", (const char *)fname) == 0) { return (void*) alGetBufferiv; }
		if (strcmp("alGenSources", (const char *)fname) == 0) { return (void*) alGenSources; }
		if (strcmp("alDeleteSources", (const char *)fname) == 0) { return (void*) alDeleteSources; }
		if (strcmp("alIsSource", (const char *)fname) == 0) { return (void*) alIsSource; }
		if (strcmp("alSourcef", (const char *)fname) == 0) { return (void*) alSourcef; }
		if (strcmp("alSourcefv", (const char *)fname) == 0) { return (void*) alSourcefv; }
		if (strcmp("alSource3f", (const char *)fname) == 0) { return (void*) alSource3f; }
		if (strcmp("alSourcei", (const char *)fname) == 0) { return (void*) alSourcei; }
		if (strcmp("alSourceiv", (const char *)fname) == 0) { return (void*) alSourceiv; }
		if (strcmp("alSource3i", (const char *)fname) == 0) { return (void*) alSource3i; }
		if (strcmp("alGetSourcef", (const char *)fname) == 0) { return (void*) alGetSourcef; }
		if (strcmp("alGetSourcefv", (const char *)fname) == 0) { return (void*) alGetSourcefv; }
		if (strcmp("alGetSource3f", (const char *)fname) == 0) { return (void*) alGetSource3f; }
		if (strcmp("alGetSourcei", (const char *)fname) == 0) { return (void*) alGetSourcei; }
		if (strcmp("alGetSourceiv", (const char *)fname) == 0) { return (void*) alGetSourceiv; }
		if (strcmp("alGetSource3i", (const char *)fname) == 0) { return (void*) alGetSource3i; }
		if (strcmp("alSourcePlay", (const char *)fname) == 0) { return (void*) alSourcePlay; }
		if (strcmp("alSourcePause", (const char *)fname) == 0) { return (void*) alSourcePause; }
		if (strcmp("alSourceStop", (const char *)fname) == 0) { return (void*) alSourceStop; }
		if (strcmp("alSourceRewind", (const char *)fname) == 0) { return (void*) alSourceRewind; }
		if (strcmp("alSourcePlayv", (const char *)fname) == 0) { return (void*) alSourcePlayv; }
		if (strcmp("alSourcePausev", (const char *)fname) == 0) { return (void*) alSourcePausev; }
		if (strcmp("alSourceStopv", (const char *)fname) == 0) { return (void*) alSourceStopv; }
		if (strcmp("alSourceRewindv", (const char *)fname) == 0) { return (void*) alSourceRewindv; }
		if (strcmp("alSourceQueueBuffers", (const char *)fname) == 0) { return (void*) alSourceQueueBuffers; }
		if (strcmp("alSourceUnqueueBuffers", (const char *)fname) == 0) { return (void*) alSourceUnqueueBuffers; }
		if (strcmp("alListenerf", (const char *)fname) == 0) { return (void*) alListenerf; }
		if (strcmp("alListenerfv", (const char *)fname) == 0) { return (void*) alListenerfv; }
		if (strcmp("alListener3f", (const char *)fname) == 0) { return (void*) alListener3f; }
		if (strcmp("alListeneri", (const char *)fname) == 0) { return (void*) alListeneri; }
		if (strcmp("alListeneriv", (const char *)fname) == 0) { return (void*) alListeneriv; }
		if (strcmp("alListener3i", (const char *)fname) == 0) { return (void*) alListener3i; }
		if (strcmp("alGetListenerf", (const char *)fname) == 0) { return (void*) alGetListenerf; }
		if (strcmp("alGetListenerfv", (const char *)fname) == 0) { return (void*) alGetListenerfv; }
		if (strcmp("alGetListener3f", (const char *)fname) == 0) { return (void*) alGetListener3f; }
		if (strcmp("alGetListeneri", (const char *)fname) == 0) { return (void*) alGetListeneri; }
		if (strcmp("alGetListeneriv", (const char *)fname) == 0) { return (void*) alGetListeneriv; }
		if (strcmp("alGetListener3i", (const char *)fname) == 0) { return (void*) alGetListener3i; }
		if (strcmp("alDistanceModel", (const char *)fname) == 0) { return (void*) alDistanceModel; }
		if (strcmp("alDopplerFactor", (const char *)fname) == 0) { return (void*) alDopplerFactor; }
		if (strcmp("alDopplerVelocity", (const char *)fname) == 0) { return (void*) alDopplerVelocity; }
		if (strcmp("alSpeedOfSound", (const char *)fname) == 0) { return (void*) alSpeedOfSound; }
		if (strcmp("alGetString", (const char *)fname) == 0) { return (void*) alGetString; }
		if (strcmp("alGetEnumValue", (const char *)fname) == 0) { return (void*) alGetEnumValue; }
		if (strcmp("alGetBoolean", (const char *)fname) == 0) { return (void*) alGetBoolean; }
		if (strcmp("alGetBooleanv", (const char *)fname) == 0) { return (void*) alGetBooleanv; }
		if (strcmp("alGetFloat", (const char *)fname) == 0) { return (void*) alGetFloat; }
		if (strcmp("alGetFloatv", (const char *)fname) == 0) { return (void*) alGetFloatv; }
		if (strcmp("alGetDouble", (const char *)fname) == 0) { return (void*) alGetDouble; }
		if (strcmp("alGetDoublev", (const char *)fname) == 0) { return (void*) alGetDoublev; }
		if (strcmp("alGetInteger", (const char *)fname) == 0) { return (void*) alGetInteger; }
		if (strcmp("alGetIntegerv", (const char *)fname) == 0) { return (void*) alGetIntegerv; }
		if (strcmp("alGetProcAddress", (const char *)fname) == 0) { return (void*) alGetProcAddress; }
		if (strcmp("alIsExtensionPresent", (const char *)fname) == 0) { return (void*) alIsExtensionPresent; }
		if (strcmp("alDisable", (const char *)fname) == 0) { return (void*) alDisable; }
		if (strcmp("alEnable", (const char *)fname) == 0) { return (void*) alEnable; }
		if (strcmp("alIsEnabled", (const char *)fname) == 0) { return (void*) alIsEnabled; }

		// Capture Extension
		if (strcmp("alcCaptureOpenDevice", (const char *)fname) == 0) { return (void*) alcCaptureOpenDevice; }
		if (strcmp("alcCaptureCloseDevice", (const char *)fname) == 0) { return (void*) alcCaptureCloseDevice; }
		if (strcmp("alcCaptureStart", (const char *)fname) == 0) { return (void*) alcCaptureStart; }
		if (strcmp("alcCaptureStop", (const char *)fname) == 0) { return (void*) alcCaptureStop; }
		if (strcmp("alcCaptureSamples", (const char *)fname) == 0) { return (void*) alcCaptureSamples; }

		// OSX Extension
		if (strcmp("alcMacOSXRenderingQuality", (const char *)fname) == 0) { return (void*) alcMacOSXRenderingQuality; }
		if (strcmp("alMacOSXRenderChannelCount", (const char *)fname) == 0) { return (void*) alMacOSXRenderChannelCount; }
		if (strcmp("alcMacOSXMixerMaxiumumBusses", (const char *)fname) == 0) { return (void*) alcMacOSXMixerMaxiumumBusses; }
		if (strcmp("alcMacOSXMixerOutputRate", (const char *)fname) == 0) { return (void*) alcMacOSXMixerOutputRate; }
		if (strcmp("alcMacOSXGetRenderingQuality", (const char *)fname) == 0) { return (void*) alcMacOSXGetRenderingQuality; }
		if (strcmp("alMacOSXGetRenderChannelCount", (const char *)fname) == 0) { return (void*) alMacOSXGetRenderChannelCount; }
		if (strcmp("alcMacOSXGetMixerMaxiumumBusses", (const char *)fname) == 0) { return (void*) alcMacOSXGetMixerMaxiumumBusses; }
		if (strcmp("alcMacOSXGetMixerOutputRate", (const char *)fname) == 0) { return (void*) alcMacOSXGetMixerOutputRate; }

		// Buffer Static Extension
		if (strcmp("alBufferDataStatic", (const char *)fname) == 0) { return (void*) alBufferDataStatic; }

		// Source Notifications Extension
		if (strcmp("alSourceAddNotification", (const char *)fname) == 0) { return (ALenum*) alSourceAddNotification; }
		if (strcmp("alSourceRemoveNotification", (const char *)fname) == 0) { return (ALvoid*) alSourceRemoveNotification; }		

		// ASA Extension
		if (strcmp("alcASASetListener", (const char *)fname) == 0) { return (void*) alcASASetListener; }
		if (strcmp("alcASAGetListener", (const char *)fname) == 0) { return (void*) alcASAGetListener; }
		if (strcmp("alcASASetSource", (const char *)fname) == 0) { return (void*) alcASASetSource; }
		if (strcmp("alcASAGetSource", (const char *)fname) == 0) { return (void*) alcASAGetSource; }
        
        // Source Spatialization Extension
        if (strcmp("alSourceRenderingQuality", (const char *)fname) == 0) { return (void*) alSourceRenderingQuality; }
        if (strcmp("alSourceGetRenderingQuality", (const char *)fname) == 0) { return (void*) alSourceGetRenderingQuality; }
        
        // Output Capturer Extension
        if (strcmp("alcOutputCapturerPrepare", (const char *)fname) == 0) { return (void*) alcOutputCapturerPrepare; }
        if (strcmp("alcOutputCapturerStart", (const char *)fname) == 0) { return (void*) alcOutputCapturerStart; }
        if (strcmp("alcOutputCapturerStop", (const char *)fname) == 0) { return (void*) alcOutputCapturerStop; }
        if (strcmp("alcOutputCapturerAvailableSamples", (const char *)fname) == 0) { return (void*) alcOutputCapturerAvailableSamples; }
        if (strcmp("alcOutputCapturerSamples", (const char *)fname) == 0) { return (void*) alcOutputCapturerSamples; }
		
	}

	return NULL;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALboolean AL_APIENTRY alIsEnvironmentIASIG (ALuint environment)
{
#if LOG_API_USAGE
	DebugMessage("***** alIsEnvironmentIASIG");
#endif
	return AL_FALSE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALboolean AL_APIENTRY alIsExtensionPresent( const ALchar* extname )
{
#if LOG_API_USAGE
	DebugMessageN1("alIsExtensionPresent function name = %s", extname);
#endif
    
	ALboolean	returnValue = AL_FALSE;
	
	if (extname == NULL)
		SetDeviceError(gCurrentDevice, ALC_INVALID_VALUE);
	else
	{
        // first compare to see if the strings match
        if(strstr(GetALExtensionList(), extname) != NULL)
        {
            returnValue = AL_TRUE;
        }    
        else
        {
            //convert the extension base to upper case
            ALchar* extbase = GetALExtensionList();
            ALchar* extbaseUpper = (ALchar*)calloc(1, (strlen(extbase)+1)*sizeof(ALchar));
            if (extbaseUpper)
            {
                for (unsigned int i=0; i < strlen(extbase); i++)
                {
                    extbaseUpper[i] = toupper(extbase[i]);
                }
                
                ALchar* extnameUpper = (ALchar*)calloc(1, (strlen(extname)+1)*sizeof(ALchar));
                if (extnameUpper)
                {
                    for (unsigned int i=0; i < strlen(extname); i++)
                    {
                        extnameUpper[i] = toupper(extname[i]);
                    }

                    //compare the strings after having converted both to upper case
                    if (strstr(extbaseUpper, extnameUpper) != NULL) {
                        returnValue = AL_TRUE;
                    }

                    free(extnameUpper);
                }
                
                free(extbaseUpper);
            }
         }
	}
    
	return returnValue;    // extension not present in this implementation
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alDisable (ALenum capability)
{
#if LOG_API_USAGE
	DebugMessageN1("alDisable--> capability = 0x%X", capability);
#endif
	
	switch (capability)
	{
		case ALC_MAC_OSX_CONVERT_DATA_UPON_LOADING:
			gConvertBufferNow = false;
			break;
		default:
			alSetError(AL_INVALID_VALUE);
			break;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alEnable (ALenum capability)
{
#if LOG_API_USAGE
	DebugMessageN1("alEnable--> capability = 0x%X", capability);
#endif
	
	switch(capability)
	{
		case ALC_MAC_OSX_CONVERT_DATA_UPON_LOADING:
			gConvertBufferNow = true;
			break;
		default:
			alSetError(AL_INVALID_VALUE);
			break;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALboolean AL_APIENTRY alIsEnabled(ALenum capability)
{
#if LOG_API_USAGE
	DebugMessageN1("alIsEnabled--> capability = 0x%X", capability);
#endif
	switch(capability)
	{
		case ALC_MAC_OSX_CONVERT_DATA_UPON_LOADING:
			return (gConvertBufferNow);
			break;
		default:
			break;
	}
	return (false);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ALC_EXT_MAC_OSX
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OSX Extension *****
ALC_API ALvoid alcMacOSXRenderingQuality (ALint value)
{
#if LOG_API_USAGE
	DebugMessageN1("alcOSXRenderingQuality--> value = %ld", (long int) value);
#endif
	alSetInteger(ALC_SPATIAL_RENDERING_QUALITY, value);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid alMacOSXRenderChannelCount (ALint value)
{
#if LOG_API_USAGE
	DebugMessageN1("alOSXRenderChannelCount--> value = %ld", (long int) value);
#endif
	alSetInteger(ALC_RENDER_CHANNEL_COUNT, value);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid alcMacOSXMixerMaxiumumBusses (ALint value)
{
#if LOG_API_USAGE
	DebugMessageN1("alcOSXMixerMaxiumumBusses--> value = %ld", (long int) value);
#endif
	alSetInteger(ALC_MIXER_MAXIMUM_BUSSES, value);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid alcMacOSXMixerOutputRate(ALdouble value)
{
#if LOG_API_USAGE
	DebugMessageN1("alcOSXMixerOutputRate--> value = %.f2", value);
#endif
	alSetDouble(ALC_MIXER_OUTPUT_RATE, value);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALint alcMacOSXGetRenderingQuality ()
{
#if LOG_API_USAGE
	DebugMessage("alcOSXGetRenderingQuality-->");
#endif
	return alGetInteger(ALC_SPATIAL_RENDERING_QUALITY);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALint alMacOSXGetRenderChannelCount ()
{
#if LOG_API_USAGE
	DebugMessage("alOSXGetRenderChannelCount-->");
#endif
	return alGetInteger(ALC_RENDER_CHANNEL_COUNT);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALint alcMacOSXGetMixerMaxiumumBusses ()
{
#if LOG_API_USAGE
	DebugMessage("alcOSXGetMixerMaxiumumBusses-->");
#endif
	return alGetInteger(ALC_MIXER_MAXIMUM_BUSSES);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALdouble alcMacOSXGetMixerOutputRate ()
{
#if LOG_API_USAGE
	DebugMessage("alcMacOSXGetMixerOutputRate-->");
#endif
	return alGetDouble(ALC_MIXER_OUTPUT_RATE);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid	AL_APIENTRY	alBufferDataStatic (ALint bid, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq)
{
#if LOG_BUFFER_USAGE
	DebugMessageN4("alBufferDataStatic-->  buffer %ld : %s : %ld bytes : %ldHz", (long int) bid, GetFormatString(format), (long int) size, (long int) freq);
#endif

	OALBuffer *oalBuffer = NULL;
	
	try {
		oalBuffer = ProtectBufferObject(bid);
        if (data==NULL) throw ((OSStatus) AL_INVALID_VALUE);
        if (size<=0) throw ((OSStatus) AL_INVALID_VALUE);
        
		oalBuffer->AddAudioDataStatic((char*)data, size, format, freq);
    }
    catch (OSStatus     result) {
		DebugMessageN5("ERROR: alBufferDataStatic FAILED: buffer %ld : %s : %ld bytes : %ldHz error = %s", (long int) bid, GetFormatString(format), (long int) size, (long int) freq,  alGetString(result));
        alSetError(result);
    }
    catch (...) {
		DebugMessageN4("ERROR: alBufferDataStatic FAILED: buffer %ld : %s : %ld bytes : %ldHz", (long int) bid, GetFormatString(format), (long int) size, (long int) freq);
        alSetError(AL_INVALID_OPERATION);
	}
	
	ReleaseBufferObject(oalBuffer);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AL_EXT_SOURCE_SPATIALIZATION
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Source Spatialization Ext*****
AL_API ALvoid alSourceRenderingQuality (ALuint sid, ALint value)
{
#if LOG_API_USAGE
	DebugMessageN1("alSourceRenderingQuality--> value = %ld", (long int) value);
#endif
    
	OALSource	*oalSource = ProtectSourceObjectInCurrentContext(sid);
    if (oalSource)
        oalSource->SetRenderQuality(value);
    
	ReleaseSourceObject(oalSource);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALint alSourceGetRenderingQuality (ALuint sid)
{
#if LOG_API_USAGE
	DebugMessage("alSourceGetRenderingQuality-->");
#endif
    
    ALint		outData = 0;
	OALSource	*oalSource = ProtectSourceObjectInCurrentContext(sid);
    if (oalSource)
        outData = oalSource->GetRenderQuality();
    
	ReleaseSourceObject(oalSource);
    return outData;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AL_EXT_SOURCE_NOTIFICATIONS
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Source Notifications Ext*****

AL_API ALenum AL_APIENTRY alSourceAddNotification (ALuint sid, ALuint notificationID, alSourceNotificationProc notifyProc, ALvoid* userData)
{
#if LOG_API_USAGE
	DebugMessage("alSourceAddNotification-->");
#endif

	if (notifyProc == NULL)
		return AL_INVALID_VALUE;
	
	ALenum		result = AL_NO_ERROR;
	OALSource	*oalSource = NULL;
	
	try {

		oalSource = ProtectSourceObjectInCurrentContext(sid);
		if (oalSource)
			result = oalSource->AddNotification(notificationID, notifyProc, userData);
		else
			result = (OSStatus) AL_INVALID_VALUE;
	
		if (result)
		{
			DebugMessageN3("ERROR: alSourceAddNotification FAILED: source: %ld : proc: %p : userData: %p", (long int) sid, notifyProc, userData);
		}
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourceAddNotification FAILED = %s\n", alGetString(result));
	}
	catch (...) {
		DebugMessage("ERROR: alSourceAddNotification FAILED");
		result = AL_INVALID_OPERATION;
	}

	ReleaseSourceObject(oalSource);
	
	return result;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alSourceRemoveNotification (ALuint sid, ALuint notificationID, alSourceNotificationProc notifyProc, ALvoid* userData)
{
#if LOG_API_USAGE
	DebugMessage("alSourceRemoveNotification-->");
#endif

	if (notifyProc == NULL)
		return;
	
	OALSource	*oalSource = NULL;

	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);
		if (oalSource)
			oalSource->RemoveNotification(notificationID, notifyProc, userData);
	}
	catch (OSStatus      result) {
		DebugMessageN1("ERROR: alSourceRemoveNotification FAILED = %s\n", alGetString(result));
	}
	catch (...) {
		DebugMessage("ERROR: alSourceRemoveNotification FAILED");
	}
	
	ReleaseSourceObject(oalSource);	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// APPLE ENVIRONMENTAL AUDIO
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
#pragma mark ***** ASA Extension *****
ALC_API ALenum  alcASAGetSource(ALuint property, ALuint sid, ALvoid *data, ALuint* dataSize)
{
#if LOG_ASA_USAGE
	DebugMessageN2("ASAGetSource--> source %ld : property %s", (long int) sid, GetALCAttributeString(property));
#endif

	if (Get3DMixerVersion() < k3DMixerVersion_2_2)
		return  AL_INVALID_OPERATION;
	
	ALCenum		err = ALC_NO_ERROR;
	OALSource*	oalSource = NULL;
			
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

        switch (property) 
        {
            case ALC_ASA_REVERB_SEND_LEVEL:
				if (*dataSize < sizeof(ALfloat))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
                *(ALfloat*)data = oalSource->GetReverbSendLevel();
                break;

            case ALC_ASA_OCCLUSION:
				if (*dataSize < sizeof(ALfloat))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
                *(ALfloat*)data =  oalSource->GetOcclusion();
                break;

            case ALC_ASA_OBSTRUCTION:
				if (*dataSize < sizeof(ALfloat))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
                *(ALfloat*)data =  oalSource->GetObstruction();
                break;
            case ALC_ASA_ROGER_BEEP_ENABLE:
				if(!IsRogerBeepPresent() || (*dataSize < sizeof(ALboolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALboolean);
                *(ALboolean*)data =  oalSource->GetRogerBeepEnable();
                break;
            case ALC_ASA_ROGER_BEEP_ON:
				if(!IsRogerBeepPresent() || (*dataSize < sizeof(ALboolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALboolean);
                *(ALboolean*)data =  oalSource->GetRogerBeepOn();
                break;
            case ALC_ASA_ROGER_BEEP_GAIN:
				if(!IsRogerBeepPresent() || (*dataSize < sizeof(ALfloat)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
                *(ALfloat*)data =  oalSource->GetRogerBeepGain();
                break;
            case ALC_ASA_ROGER_BEEP_SENSITIVITY:
				if(!IsRogerBeepPresent() || (*dataSize < sizeof(ALint)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALint);
                *(ALint*)data =  oalSource->GetRogerBeepSensitivity();
                break;
            case ALC_ASA_ROGER_BEEP_TYPE:
				if(!IsRogerBeepPresent() || (*dataSize < sizeof(ALint)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALint);
                *(ALint*)data =  oalSource->GetRogerBeepType();
                break;		
            case ALC_ASA_DISTORTION_ENABLE:
				if(!IsDistortionPresent() || (*dataSize < sizeof(ALboolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALboolean);
                *(ALboolean*)data =  oalSource->GetDistortionEnable();
                break;			
            case ALC_ASA_DISTORTION_ON:
				if(!IsDistortionPresent() || (*dataSize < sizeof(ALboolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALboolean);
                *(ALboolean*)data =  oalSource->GetDistortionOn();
                break;			
            case ALC_ASA_DISTORTION_MIX:
				if(!IsDistortionPresent() || (*dataSize < sizeof(ALfloat)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
                *(ALfloat*)data =  oalSource->GetDistortionMix();
                break;			
            case ALC_ASA_DISTORTION_TYPE:
				if(!IsDistortionPresent() || (*dataSize < sizeof(ALint)))
					throw (OSStatus) AL_INVALID_OPERATION;
				*dataSize = sizeof(ALint);
                *(ALint*)data =  oalSource->GetDistortionType();
                break;				

            default:
				err = AL_INVALID_NAME;
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN3("ERROR ASAGetSource FAILED--> source %ld : property %s : error = %s", (long int) sid, GetALCAttributeString(property), alGetString(result));
		err = result;
	}
    catch (...) {
		DebugMessageN3("ERROR ASAGetSource FAILED--> source %ld : property %s : error = %s", (long int) sid, GetALCAttributeString(property), alGetString(-1));
		err = AL_INVALID_OPERATION;
	}
	
	ReleaseSourceObject(oalSource);
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALenum  alcASASetSource(ALuint property, ALuint sid, ALvoid *data, ALuint dataSize)
{
#if LOG_ASA_USAGE
	DebugMessageN2("ASASetSource--> source %ld : property %s", (long int) sid, GetALCAttributeString(property));
#endif

	if (Get3DMixerVersion() < k3DMixerVersion_2_2)
		return  AL_INVALID_OPERATION;
	
	ALCenum			err = ALC_NO_ERROR;
	OALSource*		oalSource = NULL;
		
	try {
		oalSource = ProtectSourceObjectInCurrentContext(sid);

		FSRef nuRef;
        switch (property) 
        {
			// Source & Listener Attributes
            case ALC_ASA_REVERB_SEND_LEVEL:
				if (dataSize < sizeof(ALfloat))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetReverbSendLevel(*(ALfloat*)data);
                break;

            case ALC_ASA_OCCLUSION:
				if (dataSize < sizeof(ALfloat))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetOcclusion(*(ALfloat*)data);
                break;

            case ALC_ASA_OBSTRUCTION:
				if (dataSize < sizeof(ALfloat))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetObstruction(*(ALfloat*)data);
                break;
            case ALC_ASA_ROGER_BEEP_ENABLE:
				if((!IsRogerBeepPresent()) || (dataSize < sizeof(Boolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetRogerBeepEnable(*(Boolean*)data);
                break;
            case ALC_ASA_ROGER_BEEP_ON:
				if((!IsRogerBeepPresent()) || (dataSize < sizeof(Boolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetRogerBeepOn(*(Boolean*)data);
                break;
            case ALC_ASA_ROGER_BEEP_GAIN:
				if((!IsRogerBeepPresent()) || (dataSize < sizeof(ALfloat)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetRogerBeepGain(*(ALfloat*)data);
                break;
            case ALC_ASA_ROGER_BEEP_SENSITIVITY:
				if((!IsRogerBeepPresent()) || (dataSize < sizeof(ALint)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetRogerBeepSensitivity(*(ALint*)data);
                break;
            case ALC_ASA_ROGER_BEEP_TYPE:
				if((!IsRogerBeepPresent()) || (dataSize < sizeof(ALint)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetRogerBeepType(*(ALint*)data);
                break;
            case ALC_ASA_ROGER_BEEP_PRESET:
				if(!IsRogerBeepPresent())
					throw (OSStatus) AL_INVALID_OPERATION;
				if (FSPathMakeRef((UInt8 *) data , &nuRef, NULL))
					throw (OSStatus) AL_INVALID_OPERATION;
				oalSource->SetRogerBeepPreset(&nuRef);
                break;			
            case ALC_ASA_DISTORTION_ENABLE:
				if((!IsDistortionPresent()) || (dataSize < sizeof(Boolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetDistortionEnable(*(Boolean*)data);
                break;			
            case ALC_ASA_DISTORTION_ON:
				if((!IsDistortionPresent()) || (dataSize < sizeof(Boolean)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetDistortionOn(*(Boolean*)data);
                break;			
            case ALC_ASA_DISTORTION_MIX:
				if((!IsDistortionPresent()) || (dataSize < sizeof(ALfloat)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetDistortionMix(*(ALfloat*)data);
                break;			
            case ALC_ASA_DISTORTION_TYPE:
				if((!IsDistortionPresent()) || (dataSize < sizeof(ALint)))
					throw (OSStatus) AL_INVALID_OPERATION;
                oalSource->SetDistortionType(*(ALint*)data);
                break;			
            case ALC_ASA_DISTORTION_PRESET:
				if(!IsDistortionPresent())
					throw (OSStatus) AL_INVALID_OPERATION;
				if (FSPathMakeRef((UInt8 *) data , &nuRef, NULL))
					throw (OSStatus) AL_INVALID_OPERATION;
				oalSource->SetDistortionPreset(&nuRef);
                break;					
            default:
				err = AL_INVALID_NAME;
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN3("ERROR ASASetSource FAILED--> source %ld : property %s : error = %s", (long int) sid, GetALCAttributeString(property), alGetString(result));
		err = result;
	}
    catch (...) {
		DebugMessageN3("ERROR ASASetSource FAILED--> source %ld : property %s : error = %s", (long int) sid, GetALCAttributeString(property), alGetString(-1));
		err = AL_INVALID_OPERATION;
	}	
		
	ReleaseSourceObject(oalSource);

	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALenum  alcASAGetListener(ALuint property, ALvoid *data, ALuint* dataSize)
{
#if LOG_ASA_USAGE
	DebugMessageN1("ASAGetListener--> property %s", GetALCAttributeString(property));
#endif

	if (Get3DMixerVersion() < k3DMixerVersion_2_2)
		return  AL_INVALID_OPERATION;
	
	ALCenum err = ALC_NO_ERROR;
	
	OALContext* oalContext = NULL;
	
	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (property) 
        {
			case ALC_ASA_REVERB_ON:
				if (*dataSize < sizeof(ALuint))
					throw (OSStatus)AL_INVALID_OPERATION;
				*dataSize = sizeof(ALuint);
                *(ALuint*)data = oalContext->GetReverbState();
			
            case ALC_ASA_REVERB_ROOM_TYPE:
				if (*dataSize < sizeof(ALint))
					throw (OSStatus)AL_INVALID_OPERATION;
				*dataSize = sizeof(ALint);
                *(ALint*)data =  oalContext->GetReverbRoomType();
                break;

            case ALC_ASA_REVERB_GLOBAL_LEVEL:
				if (*dataSize < sizeof(ALfloat))
					throw (OSStatus)AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
                *(ALfloat*) data =  oalContext->GetReverbLevel();
                break;
			
            case ALC_ASA_REVERB_QUALITY:
				if (*dataSize < sizeof(ALint))
					throw (OSStatus)AL_INVALID_OPERATION;
				*dataSize = sizeof(ALint);
				*(ALint*) data =  oalContext->GetReverbQuality();
				break;

            case ALC_ASA_REVERB_EQ_GAIN:
				if (*dataSize < sizeof(ALfloat))
					throw (OSStatus)AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
					 *(ALfloat*) data =  oalContext->GetReverbEQGain();
				break;

            case ALC_ASA_REVERB_EQ_BANDWITH:
				if (*dataSize < sizeof(ALfloat))
					throw (OSStatus)AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
				*(ALfloat*) data =  oalContext->GetReverbEQBandwidth();
				break;

            case ALC_ASA_REVERB_EQ_FREQ:
				if (*dataSize < sizeof(ALfloat))
					throw (OSStatus)AL_INVALID_OPERATION;
				*dataSize = sizeof(ALfloat);
				*(ALfloat*) data =  oalContext->GetReverbEQFrequency();
				break;
			
			default:
				err = AL_INVALID_NAME;
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN2("ERROR ASAGetListener FAILED--> property %s : error = %s", GetALCAttributeString(property), alGetString(result));
		err = result;
	}
    catch (...) {
		DebugMessageN2("ERROR ASAGetListener FAILED--> property %s : error = %s", GetALCAttributeString(property), alGetString(-1));
        err = AL_INVALID_OPERATION;
	}
	
	ReleaseContextObject(oalContext);
	
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALenum  alcASASetListener(ALuint property, ALvoid *data, ALuint dataSize)
{
#if LOG_ASA_USAGE
	DebugMessageN1("ASAGetListener--> property %s", GetALCAttributeString(property));
#endif

	if (data == NULL)
		return AL_INVALID_VALUE;
		
	if (Get3DMixerVersion() < k3DMixerVersion_2_2)
		return  AL_INVALID_OPERATION;
	
	ALCenum err = ALC_NO_ERROR;
	
	OALContext* oalContext = NULL;
	
	try {
        oalContext = ProtectContextObject(gCurrentContext);
        switch (property) 
        {
            case ALC_ASA_REVERB_ON:
				if (dataSize < sizeof(UInt32))
					throw ((OSStatus) AL_INVALID_OPERATION);
                oalContext->SetReverbState(*(UInt32*)data);
                break;

            case ALC_ASA_REVERB_ROOM_TYPE:
				if (dataSize < sizeof(ALint))
					throw ((OSStatus) AL_INVALID_OPERATION);
                oalContext->SetReverbRoomType(*(ALint *)data);
                break;

            case ALC_ASA_REVERB_GLOBAL_LEVEL:
			{
				if (dataSize < sizeof(ALfloat))
					throw ((OSStatus) AL_INVALID_OPERATION);
			
				Float32		level = *(ALfloat *) data;
                oalContext->SetReverbLevel(level);
            }
			    break;

            case ALC_ASA_REVERB_PRESET:
			{
				if ((dataSize == 0) || (data == NULL))
					throw ((OSStatus) AL_INVALID_OPERATION);
				
				FSRef	nuRef;
				if (FSPathMakeRef((UInt8 *) data , &nuRef, NULL))
					throw ((OSStatus) AL_INVALID_OPERATION);
						
				oalContext->SetReverbPreset(&nuRef);
			}
				break;

            case ALC_ASA_REVERB_QUALITY:
			{
				UInt32		quality = *(ALint *) data;
				oalContext->SetReverbQuality(quality);
			}
				break;

            case ALC_ASA_REVERB_EQ_GAIN:
			{
				// check range of -18.0 - 18.0 or pin it anyway
				Float32		gain = *(ALfloat *) data;	
				if ((gain < -18.0) || (gain > 18.0))
					throw ((OSStatus) AL_INVALID_VALUE);
				oalContext->SetReverbEQGain(gain);
			}
				break;

            case ALC_ASA_REVERB_EQ_BANDWITH:
			{
				// check range of 0.5 - 4.0 or pin it anyway
				Float32		bandwidth = *(ALfloat *) data;	
				if ((bandwidth < 0.5) || (bandwidth > 4.0))
					throw ((OSStatus) AL_INVALID_VALUE);
				oalContext->SetReverbEQBandwidth(bandwidth);
			}
				break;

            case ALC_ASA_REVERB_EQ_FREQ:
			{
				// check range of 10.0 - 20000.0 or pin it anyway
				Float32		frequency = *(ALfloat *) data;	
				if ((frequency < 10.0) || (frequency > 20000.0))
					throw ((OSStatus) AL_INVALID_VALUE);
				oalContext->SetReverbEQFrequency(frequency);
			}
				break;
			
            default:
				err = AL_INVALID_NAME;
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN2("ERROR ASAGetListener FAILED--> property %s : error = %s", GetALCAttributeString(property), alGetString(result));
		err = result;
	}
    catch (...) {
		DebugMessageN2("ERROR ASAGetListener FAILED--> property %s : error = %s", GetALCAttributeString(property), alGetString(-1));
        err = AL_INVALID_OPERATION;
	}
	
	ReleaseContextObject(oalContext);
	
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** DEPRECATED for 1.1 *****
AL_API void	AL_APIENTRY alHint( ALenum target, ALenum mode )
{
	// Removed from headers for 1.1 but left in to avoid runtime link errors
	// Prototype has been removed from the public headers
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Never actually in official OpenAL headers because no tokens were defined to use it
// keep building for 1.0 released binary compatibility
AL_API ALvoid AL_APIENTRY alSetInteger (ALenum pname, ALint value)
{	
#if LOG_API_USAGE
	DebugMessage("***** alSetIntegeri");
#endif

	OALContext* oalContext	= NULL;
	
	try {
		switch(pname)
		{			
			case ALC_SPATIAL_RENDERING_QUALITY:
				if (IsValidRenderQuality ((UInt32) value))
				{
        			oalContext = ProtectContextObject(gCurrentContext);
					oalContext->SetRenderQuality ((UInt32) value);
				}
				break;

			case ALC_RENDER_CHANNEL_COUNT:
			{
				if (value != gRenderChannelSetting)
				{
					if (gOALDeviceMap == NULL)
						throw ((OSStatus) AL_INVALID_OPERATION);

					// it's a new setting, so make sure all open devices now use it
					// if there are no open devices, then all subsequent device creations will use this setting to start with
					uintptr_t	token;
					OALDevice	*oalDevice = NULL;
					gRenderChannelSetting = (UInt32) value;
					
					CAGuard::Locker locked(*gDeviceMapLock);

					for (UInt32 i = 0; i < gOALDeviceMap->Size(); i++)
					{
						oalDevice = gOALDeviceMap->GetDeviceByIndex(i, token);
						// this device may already be using this setting
						if ((ALint) oalDevice->GetRenderChannelSetting() != gRenderChannelSetting)
						{
							oalDevice->SetInUseFlag();
							oalDevice->SetRenderChannelSetting (gRenderChannelSetting); // SetRenderChannelSetting tells the library to walk the context list and adjust all context's for this device							
							oalDevice->ClearInUseFlag();
						}
					}
				}
			}
				break;

            case ALC_MIXER_MAXIMUM_BUSSES:
                gMaximumMixerBusCount = value;
                break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alSetInteger FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alSetInteger FAILED");
		alSetError(AL_INVALID_OPERATION); 
	}
	
	ReleaseContextObject(oalContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Never actually in official OpenAL headers because no tokens were defined to use it
// deprecated, keep building for 1.0 released binary compatibility
AL_API ALvoid AL_APIENTRY alSetDouble (ALenum pname, ALdouble value)
{	
#if LOG_API_USAGE
	DebugMessage("***** alSetDouble");
#endif
	
	try {
		switch (pname)
		{			
			case ALC_MIXER_OUTPUT_RATE:
				gMixerOutputRate = value;
				break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("ERROR: alSetDouble FAILED = %s\n", alGetString(result));
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("ERROR: alSetDouble FAILED");
		alSetError(AL_INVALID_OPERATION); 
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alPropagationSpeed (ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("***** alPropagationSpeed");
#endif
	if (value > 0.0f)
	{
	} 
    else
	{
        alSetError(AL_INVALID_VALUE);
	}
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alDistanceScale (ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("***** alDistanceScale");
#endif

	if (value > 0.0f)
	{
		// gDistanceScale = value;
	} 
    else
	{
		alSetError(AL_INVALID_VALUE);
	}
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alEnviromentiIASIG (ALuint environment, ALenum pname, ALint value)
{
#if LOG_API_USAGE
	DebugMessage("***** alEnviromentiIASIG");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alEnvironmentfIASIG (ALuint environment, ALenum pname, ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("***** alEnvironmentfIASIG");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALsizei AL_APIENTRY alGenEnvironmentIASIG (ALsizei n, ALuint *environments)
{
#if LOG_API_USAGE
	DebugMessage("***** alGenEnvironmentIASIG");
#endif
	return 0;
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AL_API ALvoid AL_APIENTRY alDeleteEnvironmentIASIG (ALsizei n, ALuint *environments)
{
#if LOG_API_USAGE
	DebugMessage("***** alDeleteEnvironmentIASIG");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Output Capturer Extension
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid  alcOutputCapturerPrepare( ALCuint inFrequency, ALCenum inFormat, ALCsizei inBuffersize )
{
	if (gOALContextMap == NULL)
		goto fail;
	
	{
        OSStatus result = noErr;
		OALContext* context = ProtectContextObject((uintptr_t) gCurrentContext);
        if (context)
        {
            result = context->OutputCapturerCreate(inFrequency, inFormat, inBuffersize);
        }
        else
        {
            result = -1;  //context doesn't exist
        }
        ReleaseContextObject(context);
        if (result) goto fail;
	}
    return;
    
fail:
	SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid  alcOutputCapturerStart()
{
	if (gOALContextMap == NULL)
		goto fail;
	
	{
        OSStatus result = noErr;
		OALContext* context = ProtectContextObject((uintptr_t) gCurrentContext);
        if (context)
        {
            result = context->OutputCapturerStart();
        }
        else
        {
            result = -1;  //context doesn't exist
        }
        ReleaseContextObject(context);
        if (result) goto fail;
	}
    return;
    
fail:
	SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid  alcOutputCapturerStop()
{
	if (gOALContextMap == NULL)
		goto fail;
	
	{
        OSStatus result = noErr;
		OALContext* context = ProtectContextObject((uintptr_t) gCurrentContext);
        if (context)
        {
            result = context->OutputCapturerStop();
        }
        else
        {
            result = -1;  //context doesn't exist
        }
        ReleaseContextObject(context);
        if (result) goto fail;
	}
    return;
    
fail:
	SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALint   alcOutputCapturerAvailableSamples()
{
    ALint numAvailableSamples = 0;
	if (gOALContextMap == NULL)
		goto fail;
	
	{
        OSStatus result = noErr;
		OALContext* context = ProtectContextObject((uintptr_t) gCurrentContext);
        if (context)
        {
            numAvailableSamples = (ALint) context->OutputCapturerAvailableFrames();
        }
        else
        {
            result = -1;  //context doesn't exist
        }
        ReleaseContextObject(context);
        if (result) goto fail;
	}
    
    return numAvailableSamples;
    
fail:
	SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
    return 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALC_API ALvoid  alcOutputCapturerSamples( ALCvoid *inBuffer, ALCsizei inSamples )
{
	if (gOALContextMap == NULL)
		goto fail;
	
	{
        OSStatus result = noErr;
		OALContext* context = ProtectContextObject((uintptr_t) gCurrentContext);
        if (context)
        {
            result = context->OutputCapturerGetFrames(inSamples, (UInt8*) inBuffer);
        }
        else
        {
            result = -1;  //context doesn't exist
        }
        ReleaseContextObject(context);
        if (result) goto fail;
	}
    return;
    
fail:
	SetDeviceError(gCurrentDevice, AL_INVALID_OPERATION);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MAYBE LATER
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCchar 					gOutputDeviceList[maxLen*2];
ALCchar 					gInputDeviceList[maxLen*2];

void	GetDefaultDeviceNameList(ALCchar*		outDeviceNameList, bool	isInput)
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
		result = AudioDeviceGetProperty(defaultDevice, 0, false, kAudioDevicePropertyDeviceName, &size, outDeviceNameList);
			THROW_RESULT
			
		outDeviceNameList[size] = '\0'; // double terminator
			
	} catch (...) {
		outDeviceNameList[0] = '\0'; // failure case, make it a zero length string
		outDeviceNameList[1] = '\0'; // failure case, make it a zero length string
	}
}

void	GetDeviceList(ALCchar*		outDeviceName, bool	inMakeInputDeviceList)
{
	UInt32		size = 0;
	OSStatus	result = noErr;
	UInt32		deviceCount = 0;
	ALCchar		*curStrPtr = outDeviceName;
	UInt32		charCount = 0;

	result = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &size, NULL);
		THROW_RESULT
	deviceCount = size / sizeof(AudioDeviceID);

	AudioDeviceID	*ids = (AudioDeviceID*) calloc(1, size);
	result = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &size, ids);
		THROW_RESULT
	
	bool deviceDoesInput;
	bool deviceDoesOutput;
	
	for (UInt32	i; i < deviceCount; i++)
	{
		size = 0;
		AudioDeviceGetPropertyInfo( ids[i], 0, true, kAudioDevicePropertyStreams, &size, NULL);
		deviceDoesInput = size > 0 ? true : false;

		size = 0;
		AudioDeviceGetPropertyInfo( ids[i], 0, false, kAudioDevicePropertyStreams, &size, NULL);
		deviceDoesOutput = size > 0 ? true : false;

		if ((inMakeInputDeviceList && deviceDoesInput) || (!inMakeInputDeviceList && deviceDoesOutput))
		{
			size = 0;
			result = AudioDeviceGetPropertyInfo( ids[i], 0, inMakeInputDeviceList, kAudioDevicePropertyDeviceName, &size, NULL);
				THROW_RESULT
			if (charCount + size > maxLen*2)
				throw -1;
			
			// get name
			result = AudioDeviceGetProperty(ids[i], 0, inMakeInputDeviceList, kAudioDevicePropertyDeviceName, &size, curStrPtr);
				THROW_RESULT

			curStrPtr +=size;
			charCount +=size;
		}
	}

	*curStrPtr = '\0';
	if(charCount == 0)
	{
		curStrPtr++;
		*curStrPtr = '\0';
	}

}
*/

