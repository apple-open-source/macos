/*
 * Copyright (c) 2014 Apple Inc. All rights reserved.
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

#ifndef _AUDIOTRACEPOINTS_H
#define _AUDIOTRACEPOINTS_H


extern UInt32   gAudioStackDebugFlags;


#ifdef __cplusplus
extern "C" {
#endif
    
#define AUDIO_SYSCTL			"debug.Audio"
#define kAudioTypeDebug         'AUDD'
#define DEBUG_UNUSED( X )	( void )( X )
    
    
    typedef struct AudioSysctlArgs
    {
        uint32_t		type;
        uint32_t		operation;
        uint32_t		debugFlags;
    } AudioSysctlArgs;
    
    enum
    {
        kAudioOperationGetFlags 	= 0,
        kAudioOperationSetFlags     = 1
    };
    
    // the following bits/masks are for use in the audio boot args
    // e.g. boot-args="audio=1" will turn on Trace Points
    // the bits can also be set with a sysctl call
    enum
    {
        kAudioEnableTracePointsBit		= 0,	// bit 0 used to turn on Trace Points when IOAudioFamily first loads (usually via an IOAudioDevice)
        
        kAudioEnableTracePointsMask			= (1 << kAudioEnableTracePointsBit),      // 0x0001
    };
    
    
    /* Kernel Tracepoints
     *
     * Kernel tracepoints are a logging mechanism reduces the size of a log-laden binary.
     * Codes are placed into a buffer, from the kernel, and picked up by a userspace
     * tool that displays a unique log message for each tracepoint. Additionally, each
     * tracepoint may contain up-to four pointer sized arguments.
     *
     * To add a tracepoint, use the code below as an example:
     * AudioTrace( kAudioTDevice, kTPAudioDeviceStart, (uintptr_t)myArgValue );
     * Next, add the corresponding tracepoint code in the audiotracer tool, using
     * the existing examples. Avoid using confidential information in the log strings.
     * Some functions have a argument counter, to signify which of the function's tracepoints
     * are actually being logged. When adding a tracepoint using an existing code, you
     * must verify that you increment this argument counter properly.
     *
     * The trace codes consist of the following:
     *
     * ----------------------------------------------------------------------
     *| Class (8)      | SubClass (8)   | AudioGroup(6) | Code (8)    |Func   |
     *| DBG_IOKIT      | DBG_IOAUDIO    |               |             |Qual(2)|
     * ----------------------------------------------------------------------
     *
     * DBG_IOKIT(05h)  DBG_IOAUDIO(24h)
     *
     * See <sys/kdebug.h> and IOTimeStamp.h for more details.
     */
    
    
    // AudioGroup (max of 64)
    enum
    {
        // Family groupings
        kAudioTAppleHDAHistoric				= 0,            // historic values from <rdar://12678851>
        kAudioTIOAudioControl				= 1,
        kAudioTIOAudioControlUserClient     = 2,
        kAudioTIOAudioDevice                = 3,
        kAudioTIOAudioEngine                = 4,
        kAudioTIOAudioEngineUserClient      = 5,
        kAudioTIOAudioLevelControl          = 6,
        kAudioTIOAudioPort                  = 7,
        kAudioTIOAudioSelectorControl       = 8,
        kAudioTIOAudioStream                = 9,
        kAudioTIOAudioToggleControl         = 10,
        kAudioTIOAudioTimeIntervalFilter    = 11,
        // reserve 12-19 for future IOAudioFamily codes
        
        kAudioTAppleHDAController           = 20,
        kAudioTAppleHDADriver               = 21,
        kAudioTAppleHDAEngine               = 22,
        kAudioTAppleHDAPath                 = 23,
        kAudioTAppleHDACodecGeneric         = 24,
        kAudioTAppleHDANode                 = 25,
        kAudioTAppleHDAFunctionGroup        = 26,
        kAudioTAppleHDAWidget               = 27,
        kAudioTAppleHDAEngineOutputDP       = 28,
        // reserve 29-34 for future AppleHDA codes
        
        kAudioTAppleUSBAudioDevice          = 35,
        kAudioTAppleUSBAudioDictionary      = 36,
        kAudioTAppleUSBAudioEngine          = 37,
        kAudioTAppleUSBAudioStream          = 38,
        kAudioTAppleUSBAudioPlugin          = 39
        
    };
    
    // Tracepoint macros.
#define AUDIO_TRACE(AudioClass, code)	( ( ( DBG_IOKIT & 0xFF ) << 24) | ( ( DBG_IOAUDIO & 0xFF ) << 16 ) | ( ( AudioClass & 0x3F ) << 10 ) | ( ( code & 0xFF ) << 2 ) )
    
#define kTPAllAudio                                 AUDIO_TRACE ( 0, 0 )
    
    // these macros are used by the tool to coalesce the various types
#define AUDIO_IOAUDIOCONTROL_TRACE(code)            AUDIO_TRACE( kAudioTIOAudioControl, code )
#define AUDIO_IOAUDIOCONTROLUSERCLIENT_TRACE(code)  AUDIO_TRACE( kAudioTIOAudioControlUserClient, code )
#define AUDIO_IOAUDIODEVICE_TRACE(code)             AUDIO_TRACE( kAudioTIOAudioDevice, code )
#define AUDIO_IOAUDIOENGINE_TRACE(code)             AUDIO_TRACE( kAudioTIOAudioEngine, code )
#define AUDIO_IOAUDIOENGINEUSERCLIENT_TRACE(code)	AUDIO_TRACE( kAudioTIOAudioEngineUserClient, code )
#define AUDIO_IOAUDIOLEVELCONTROL_TRACE(code)		AUDIO_TRACE( kAudioTIOAudioLevelControl, code )
#define AUDIO_IOAUDIOPORT_TRACE(code)               AUDIO_TRACE( kAudioTIOAudioPort, code )
#define AUDIO_IOAUDIOSELECTORCONTROL_TRACE(code)	AUDIO_TRACE( kAudioTIOAudioSelectorControl, code )
#define AUDIO_IOAUDIOSTREAM_TRACE(code)             AUDIO_TRACE( kAudioTIOAudioStream, code )
#define AUDIO_IOAUDIOTOGGLECONTROL_TRACE(code)		AUDIO_TRACE( kAudioTIOAudioToggleControl, code )
#define AUDIO_IOAUDIOTIMEINTERVAL_TRACE(code)		AUDIO_TRACE( kAudioTIOAudioTimeIntervalFilter, code )
    
#define AUDIO_APPLEHDAHISTORIC_TRACE(code)          AUDIO_TRACE( kAudioTAppleHDAHistoric, code )
#define AUDIO_APPLEHDACONTROLLER_TRACE(code)		AUDIO_TRACE( kAudioTAppleHDAController, code )
#define AUDIO_APPLEHDADRIVER_TRACE(code)            AUDIO_TRACE( kAudioTAppleHDADriver, code )
#define AUDIO_APPLEHDAENGINE_TRACE(code)            AUDIO_TRACE( kAudioTAppleHDAEngine, code )
#define AUDIO_APPLEHDAPATH_TRACE(code)              AUDIO_TRACE( kAudioTAppleHDAPath, code )
#define AUDIO_APPLEHDACODECGENERIC_TRACE(code)      AUDIO_TRACE( kAudioTAppleHDACodecGeneric, code )
#define AUDIO_APPLEHDANODE_TRACE(code)              AUDIO_TRACE( kAudioTAppleHDANode, code )
#define AUDIO_APPLEHDAFUNCTIONGROUP_TRACE(code)		AUDIO_TRACE( kAudioTAppleHDAFunctionGroup, code )
#define AUDIO_APPLEHDAWIDGET_TRACE(code)            AUDIO_TRACE( kAudioTAppleHDAWidget, code )
#define AUDIO_APPLEHDAENGINEOUTPUTDP_TRACE(code)    AUDIO_TRACE( kAudioTAppleHDAEngineOutputDP, code )
    
#define AUDIO_APPLEUSBAUDIODEVICE_TRACE(code)       AUDIO_TRACE( kAudioTAppleUSBAudioDevice, code )
#define AUDIO_APPLEUSBAUDIODICTIONARY_TRACE(code)   AUDIO_TRACE( kAudioTAppleUSBAudioDictionary, code )
#define AUDIO_APPLEUSBAUDIOENGINE_TRACE(code)       AUDIO_TRACE( kAudioTAppleUSBAudioEngine, code )
#define AUDIO_APPLEUSBAUDIOSTREAM_TRACE(code)       AUDIO_TRACE( kAudioTAppleUSBAudioStream, code )
#define AUDIO_APPLEUSBAUDIOPLUGIN_TRACE(code)       AUDIO_TRACE( kAudioTAppleUSBAudioPlugin, code )
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioControl codes (max 256)
    enum
    {
        kTPIOAudioControlCreateUserClient                   = 0,
        kTPIOAudioControlSendChangeNotification             = 1,
        kTPIOAudioControlSendQueuedNotifications            = 2,
        kTPIOAudioControlWithAttributes                     = 3,
        kTPIOAudioControlInit                               = 4,
        kTPIOAudioControlSetType                            = 5,
        kTPIOAudioControlSetSubType                         = 6,
        kTPIOAudioControlSetChannelName                     = 7,
        kTPIOAudioControlSetUsage                           = 8,
        kTPIOAudioControlSetCoreAudioPropertyID             = 9,
        kTPIOAudioControlSetReadOnlyFlags                   = 10,
        kTPIOAudioControlFree                               = 11,
        kTPIOAudioControlStart                              = 12,
        kTPIOAudioControlAttachAndStart                     = 13,
        kTPIOAudioControlStop                               = 14,
        kTPIOAudioControlSetWorkloop                        = 15,
        kTPIOAudioControl_SetValueAction                    = 16,
        kTPIOAudioControlSetValueAction                     = 17,
        kTPIOAudioControlSetValue                           = 18,
        kTPIOAudioControlUpdateValue                        = 19,
        kTPIOAudioControl_SetValue                          = 20,
        kTPIOAudioControlHardwareValueChanged               = 21,
        kTPIOAudioControlSetValueChangedHandler             = 22,
        kTPIOAudioControlSetValueChangedTarget              = 23,
        kTPIOAudioControlPerformValueChange                 = 24,
        kTPIOAudioControlGetIntValue                        = 25,
        kTPIOAudioControlGetDataBytes                       = 26,
        kTPIOAudioControlGetDataLength                      = 27,
        kTPIOAudioControlSendValueChangeNotification        = 28,
        kTPIOAudioControlSetControlID                       = 29,
        kTPIOAudioControlSetChannelID                       = 30,
        kTPIOAudioControlSetChannelNumber                   = 31,
        kTPIOAudioControlNewUserClient                      = 32,
        kTPIOAudioControlClientClosed                       = 33,
        kTPIOAudioControl_AddUserClientAction               = 34,
        kTPIOAudioControlAddUserClientAction                = 35,
        kTPIOAudioControl_RemoveUserClientAction            = 36,
        kTPIOAudioControlRemoveUserClientAction             = 37,
        kTPIOAudioControlDetachUserClientsAction            = 38,
        kTPIOAudioControlAddUserClient                      = 39,
        kTPIOAudioControlRemoveUserClient                   = 40,
        kTPIOAudioControlDetachUserClients                  = 41,
        kTPIOAudioControlSetCommandGateUsage                = 42,
        kTPIOAudioControlSetProperties                      = 43
    };
    
    
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioControlUserClient codes (max 256)
    enum
    {
        kTPIOAudioControlUserClientInitWithAudioControl     = 0,
        kTPIOAudioControlUserClientSendChangeNotification   = 1,
        kTPIOAudioControlUserClientWithAudioControl         = 2,
        kTPIOAudioControlUserClientFree                     = 3,
        kTPIOAudioControlUserClientClientClose              = 4,
        kTPIOAudioControlUserClientClientDied               = 5,
        kTPIOAudioControlUserClientRegisterNotificationPort = 6,
        kTPIOAudioControlUserClientSendValueChangeNotifiation = 7
        
    };
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioDevice codes (max 256)
    enum
    {
        kTPIOAudioDeviceInit                                = 0,
        kTPIOAudioDeviceFree                                = 1,
        kTPIOAudioDeviceInitHardware                        = 2,
        kTPIOAudioDeviceStart                               = 3,
        kTPIOAudioDeviceStop                                = 4,
        kTPIOAudioDeviceWillTerminate                       = 5,
        kTPIOAudioDeviceSetPowerState                       = 6,
        kTPIOAudioDevice_SetPowerStateAction                = 7,
        kTPIOAudioDeviceSetPowerStateAction                 = 8,
        kTPIOAudioDeviceProtectedSetPowerState              = 9,
        kTPIOAudioDeviceWaitForPendingPowerStateChange      = 10,
        kTPIOAudioDeviceInitiatePowerStateChange            = 11,
        kTPIOAudioDeviceCompletePowerStateChange            = 12,
        kTPIOAudioDeviceCompletePowerStateChangeAction      = 13,
        kTPIOAudioDeviceProtectedCompletePowerStateChange   = 14,
        kTPIOAudioDevicePerformPowerStateChange             = 15,
        kTPIOAudioDeviceAudioEngineStarting                 = 16,
        kTPIOAudioDeviceAudioEngineStopped                  = 17,
        kTPIOAudioDeviceSetDeviceName                       = 18,
        kTPIOAudioDeviceSetDeviceShortName                  = 19,
        kTPIOAudioDeviceSetManufacturerName                 = 20,
        kTPIOAudioDeviceActivateAudioEngine                 = 21,
        kTPIOAudioDeviceDeactivateAllAudioEngines           = 22,
        kTPIOAudioDeviceFlushAudioControls                  = 23,
        kTPIOAudioDeviceAddTimerEvent                       = 24,
        kTPIOAudioDeviceRemoveTimerEvent                    = 25,
        kTPIOAudioDeviceRemoveAllTimerEvents                = 26,
        kTPIOAudioDeviceTimerFired                          = 27,
        kTPIOAudioDeviceDispatchTimerEvents                 = 28,
        
    };
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioEngine codes (max 256)
    enum
    {
        kTPIOAudioEngineCreateUserClient                    = 0,
        kTPIOAudioEngineSetInputSampleOffset                = 1,
        kTPIOAudioEngineSetOutputSampleOffset               = 2,
        kTPIOAudioEngineConvertInputSamplesVBR              = 3,
        kTPIOAudioEngineSetClockDomain                      = 4,
        kTPIOAudioEngineGetStreamForID                      = 5,
        kTPIOAudioEngineGetNextStreamID                     = 6,
        kTPIOAudioEnginePerformFormatChange                 = 7,
        kTPIOAudioEngineGetStatusDescriptor                 = 8,
        kTPIOAudioEngineGetNearestStartTime                 = 9,
        kTPIOAudioEngineEraseOutputSamples                  = 10,
        kTPIOAudioEngineSetMixClipOverhead                  = 11,
        kTPIOAudioEngineCompareAudioStreams                 = 12,
        kTPIOAudioEngineCreateDictionaryFromSampleRate      = 13,
        kTPIOAudioEngineCreateSampleRateFromDictionary      = 14,
        kTPIOAudioEngineInit                                = 15,
        kTPIOAudioEngineFree                                = 16,
        kTPIOAudioEngineInitHardware                        = 17,
        kTPIOAudioEngineStart                               = 18,
        kTPIOAudioEngineStop                                = 19,
        kTPIOAudioEngineRegisterService                     = 20,
        kTPIOAudioEngineGetGlobalUniqueID                   = 21,
        kTPIOAudioEngineGetLocalUniqueID                    = 22,
        kTPIOAudioEngineSetIndex                            = 23,
        kTPIOAudioEngineSetAudioDevice                      = 24,
        kTPIOAudioEngineResetStatusBuffer                   = 25,
        kTPIOAudioEngineClearAllSampleBuffers               = 26,
        kTPIOAudioEngineNewUserClient                       = 27,
        kTPIOAudioEngineClientClosed                        = 28,
        kTPIOAudioEngine_AddUserClientAction                = 29,
        kTPIOAudioEngineAddUserClientAction                 = 30,
        kTPIOAudioEngine_RemoveUserClientAction             = 31,
        kTPIOAudioEngineRemoveUserClientAction              = 32,
        kTPIOAudioEngineDetachUserClientsAction             = 33,
        kTPIOAudioEngineAddUserClient                       = 34,
        kTPIOAudioEngineRemoveUserClient                    = 35,
        kTPIOAudioEngineDetachUserClients                   = 36,
        kTPIOAudioEngineStartClient                         = 37,
        kTPIOAudioEngineStopClient                          = 38,
        kTPIOAudioEngineIncrementActiveUserClients          = 39,
        kTPIOAudioEngineDecrementActiveUserClient           = 40,
        kTPIOAudioEngineAddAudioStream                      = 41,
        kTPIOAudioEngineDetachAudioStreams                  = 42,
        kTPIOAudioEngineLockAllStreams                      = 43,
        kTPIOAudioEngineUnlockAllStreams                    = 44,
        kTPIOAudioEngineGetAudioStream                      = 45,
        kTPIOAudioEngineUpdateChannelNumbers                = 46,
        kTPIOAudioEngineStartAudioEngine                    = 47,
        kTPIOAudioEngineStopAudioEngine                     = 48,
        kTPIOAudioEnginePauseAudioEngine                    = 49,
        kTPIOAudioEngineResumeAudioEngine                   = 50,
        kTPIOAudioEngineGetStatus                           = 51,
        kTPIOAudioEngineSetNumSampleFramesPerBuffer         = 52,
        kTPIOAudioEngineGetNumSampleFramesPerBuffer         = 53,
        kTPIOAudioEngineGetState                            = 54,
        kTPIOAudioEngineSetState                            = 55,
        kTPIOAudioEngineGetSampleRate                       = 56,
        kTPIOAudioEngineSetSampleRate                       = 57,
        kTPIOAudioEngineHardwareSampleRateChanged           = 58,
        kTPIOAudioEngineSetSampleLatency                    = 59,
        kTPIOAudioEngineSetOutputSampleLatency              = 60,
        kTPIOAudioEngineSetInputSampleLatency               = 61,
        kTPIOAudioEngineSetSampleOffset                     = 62,
        kTPIOAudioEngineSetRunEraseHead                     = 63,
        kTPIOAudioEngineGetRunEraseHead                     = 64,
        kTPIOAudioEngineGetTimerInterval                    = 65,
        kTPIOAudioEngineTimerCallback                       = 66,
        kTPIOAudioEngineTimerFired                          = 67,
        kTPIOAudioEnginePerformErase                        = 68,
        kTPIOAudioEngineStopEngineAtPosition                = 69,
        kTPIOAudioEnginePerformFlush                        = 70,
        kTPIOAudioEngineAddTimer                            = 71,
        kTPIOAudioEngineRemoveTimer                         = 72,
        kTPIOAudioEngineClipOutputSamples                   = 73,
        kTPIOAudioEngineConvertInputSamples                 = 74,
        kTPIOAudioEngineTakeTimeStamp                       = 75,
        kTPIOAudioEngineGetLoopCountAndTimeStamp            = 76,
        kTPIOAudioEngineCalculateSampleTimeout              = 77,
        kTPIOAudioEngineSendFormatChangeNotification        = 78,
        kTPIOAudioEngineSendNotification                    = 79,
        kTPIOAudioEngineBeginConfigurationChange            = 80,
        kTPIOAudioEngineCompleteConfigurationChange         = 81,
        kTPIOAudioEngineCancelConfigurationChange           = 82,
        kTPIOAudioEngineAddDefaultAudioControl              = 83,
        kTPIOAudioEngineRemoveDefaultAudioControl           = 84,
        kTPIOAudioEngineRemoveAllDefaultAudioControls       = 85,
        kTPIOAudioEngineSetWorkloopOnAllAudioControls       = 86,
        kTPIOAudioEngineSetCommandGateUsage                 = 87,
        kTPIOAudioEngineWaitForEngineResume                 = 88,
        kTPIOAudioEngineMixOutputSamples                    = 89
    };
    
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioEngineUserClient codes (max 256)
    enum
    {
        kTPIOAudioEngineUserClientInitWithAudioEngine           = 0,
        kTPIOAudioEngineUserClientSafeRegisterClientBuffer      = 1,
        kTPIOAudioEngineUserClientSafeRegisterClientBuffer64    = 2,
        kTPIOAudioEngineUserClientRegisterClientParameterBuffer = 3,
        kTPIOAudioEngineUserClientFindExtendedInfo              = 4,
        kTPIOAudioEngineUserClientFindExtendedInfo64            = 5,
        kTPIOAudioEngineUserClientGetNearestStartTime           = 6,
        kTPIOAudioEngineUserClient_GetNearestStartTimeAction    = 7,
        kTPIOAudioEngineUserClientGetNearestStartTimeAction     = 8,
        kTPIOAudioEngineUserClientGetClientNearestStartTime     = 9,
        kTPIOAudioEngineUserClientWithAudioEngine               = 10,
        kTPIOAudioEngineUserClientFree                          = 11,
        kTPIOAudioEngineUserClientFreeClientBufferSetList       = 12,
        kTPIOAudioEngineUserClientFreeClientBuffer              = 13,
        kTPIOAudioEngineUserClientStop                          = 14,
        kTPIOAudioEngineUserClientClientClose                   = 15,
        kTPIOAudioEngineUserClientClientDied                    = 16,
        kTPIOAudioEngineUserClient_CloseClientAction            = 17,
        kTPIOAudioEngineUserClientCloseClientAction             = 18,
        kTPIOAudioEngineUserClientCloseClient                   = 19,
        kTPIOAudioEngineUserClientSetOnline                     = 20,
        kTPIOAudioEngineUserClientLockBuffers                   = 21,
        kTPIOAudioEngineUserClientUnlockBuffers                 = 22,
        kTPIOAudioEngineUserClientClientMemoryForType           = 23,
        kTPIOAudioEngineUserClientGetExternalMemoryForIndex     = 24,
        kTPIOAudioEngineUserClientGetExternalTrapForIndex       = 25,
        kTPIOAudioEngineUserClientRegisterNotificationPort      = 26,
        kTPIOAudioEngineUserClient_RegisterNotificationAction   = 27,
        kTPIOAudioEngineUserClientRegisterNotificationAction    = 28,
        kTPIOAudioEngineUserClientRegisterNotification          = 29,
        kTPIOAudioEngineUserClientExternalMethod                = 30,
        kTPIOAudioEngineUserClientRegisterBuffer                = 31,
        kTPIOAudioEngineUserClientRegisterBuffer64              = 32,
        kTPIOAudioEngineUserClientUnregisterBuffer              = 33,
        kTPIOAudioEngineUserClientUnregisterBuffer64            = 34,
        kTPIOAudioEngineUserClient_RegisterBufferAction         = 35,
        kTPIOAudioEngineUserClientRegisterBufferAction          = 36,
        kTPIOAudioEngineUserClient_UnregisterBufferAction       = 37,
        kTPIOAudioEngineUserClientUnregisterBufferAction        = 38,
        kTPIOAudioEngineUserClientRegisterClientBuffer          = 39,
        kTPIOAudioEngineUserClientRegisterClientBuffer64        = 40,
        kTPIOAudioEngineUserClientUnregisterClientBuffer        = 41,
        kTPIOAudioEngineUserClientUnregisterClientBuffer64      = 42,
        kTPIOAudioEngineUserClientFindBufferSet                 = 43,
        kTPIOAudioEngineUserClientRemoveBufferSet               = 44,
        kTPIOAudioEngineUserClientPerformClientIO               = 45,
        kTPIOAudioEngineUserClientPerformClientOutput           = 46,
        kTPIOAudioEngineUserClientPerformClientInput            = 47,
        kTPIOAudioEngineUserClientPerformWatchdogOutput         = 48,
        kTPIOAudioEngineUserClientGetConnectionID               = 49,
        kTPIOAudioEngineUserClientClientStart                   = 50,
        kTPIOAudioEngineUserClientClientStop                    = 51,
        kTPIOAudioEngineUserClient_StartClientAction            = 52,
        kTPIOAudioEngineUserClientStartClientAction             = 53,
        kTPIOAudioEngineUserClient_StopClientAction             = 54,
        kTPIOAudioEngineUserClientStopClientAction              = 55,
        kTPIOAudioEngineUserClientStartClient                   = 56,
        kTPIOAudioEngineUserClientStopClient                    = 57,
        kTPIOAudioEngineUserClientSendFormatChangeNotification  = 58,
        kTPIOAudioEngineUserClientSendNotification              = 59,
        kTPIOAudioEngineUserClientSetCommandGateUsage           = 60
        
    };
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioLevelControl codes (max 256)
    enum
    {
        kTPIOAudioLevelControlCreatePassThroughVolumeControl    = 0,
        kTPIOAudioLevelControlSetLinearScale                    = 1,
        kTPIOAudioLevelControlCreate                            = 2,
        kTPIOAudioLevelControlCreateVolumeControl               = 3,
        kTPIOAudioLevelControlInit                              = 4,
        kTPIOAudioLevelControlFree                              = 5,
        kTPIOAudioLevelControlSetMinValue                       = 6,
        kTPIOAudioLevelControlGetMinValue                       = 7,
        kTPIOAudioLevelControlSetMaxValue                       = 8,
        kTPIOAudioLevelControlGetMaxValue                       = 9,
        kTPIOAudioLevelControlSetMinDB                          = 10,
        kTPIOAudioLevelControlGetMinDB                          = 11,
        kTPIOAudioLevelControlSetMaxDB                          = 12,
        kTPIOAudioLevelControlGetMaxDB                          = 13,
        kTPIOAudioLevelControlAddNegativeInfinity               = 14,
        kTPIOAudioLevelControlValidateValue                     = 15
    };
    
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioPort codes (max 256)
    enum
    {
        kTPIOAudioPortWithAttributes                            = 0,
        kTPIOAudioPortInitWithAttributes                        = 1,
        kTPIOAudioPortFree                                      = 2,
        kTPIOAudioPortSetType                                   = 3,
        kTPIOAudioPortSetSubType                                = 4,
        kTPIOAudioPortSetName                                   = 5,
        kTPIOAudioPortStart                                     = 6,
        kTPIOAudioPortStop                                      = 7,
        kTPIOAudioPortRegisterService                           = 8,
        kTPIOAudioPortGetAudioDevice                            = 9,
        kTPIOAudioPortAddAudioControl                           = 10,
        kTPIOAudioPortDeactivateAudioControls                   = 11
    };
    
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioSelectorControl codes (max 256)
    enum
    {
        kTPIOAudioSelectorControlCreateOutputClockSelector      = 0,
        kTPIOAudioSelectorControlCreateInputClockSelector       = 1,
        kTPIOAudioSelectorControlCreateOutputSelector           = 2,
        kTPIOAudioSelectorControlRemoveAvailableSelection       = 3,
        kTPIOAudioSelectorControlReplaceAvailableSelection      = 4,
        kTPIOAudioSelectorControlCreate                         = 5,
        kTPIOAudioSelectorControlCreateInputSelector            = 6,
        kTPIOAudioSelectorControlInit                           = 7,
        kTPIOAudioSelectorControlFree                           = 8,
        kTPIOAudioSelectorControlAddAvailableSelection          = 9,
        kTPIOAudioSelectorControlValueExists                    = 10,
        kTPIOAudioSelectorControlValidateValue                  = 11
    };
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioStream codes (max 256)
    enum
    {
        kTPIOAudioStreamSafeLogError                            = 0,
        kTPIOAudioStreamValidateFormat                          = 1,
        kTPIOAudioStreamGetFormatExtension                      = 2,
        kTPIOAudioStreamSetFormat                               = 3,
        kTPIOAudioStreamAddAvailableFormat                      = 4,
        kTPIOAudioStreamSetTerminalType                         = 5,
        kTPIOAudioStreamMixOutputSamples                        = 6,
        kTPIOAudioStreamSetSampleLatency                        = 7,
        kTPIOAudioStreamGetNumSampleFramesRead                  = 8,
        kTPIOAudioStreamSetDefaultNumSampleFramesRead           = 9,
        kTPIOAudioStreamInitKeys                                = 10,
        kTPIOAudioStreamCreateDictionaryFromFormat              = 11,
        kTPIOAudioStreamCreateFormatFromDictionary              = 12,
        kTPIOAudioStreamInitWithAudioEngine                     = 13,
        kTPIOAudioStreamFree                                    = 14,
        kTPIOAudioStreamStop                                    = 15,
        kTPIOAudioStreamSetProperties                           = 16,
        kTPIOAudioStreamSetDirection                            = 17,
        kTPIOAudioStreamGetDirection                            = 18,
        kTPIOAudioStreamSetSampleBuffer                         = 19,
        kTPIOAudioStreamGetSampleBuffer                         = 20,
        kTPIOAudioStreamGetSampleBufferSize                     = 21,
        kTPIOAudioStreamSetMixBuffer                            = 22,
        kTPIOAudioStreamGetMixBuffer                            = 23,
        kTPIOAudioStreamGetMixBufferSize                        = 24,
        kTPIOAudioStreamNumSampleFramesPerBufferChanged         = 25,
        kTPIOAudioStreamClearSampleBuffer                       = 26,
        kTPIOAudioStreamSetIOFunction                           = 27,
        kTPIOAudioStreamSetIOFunctionList                       = 28,
        kTPIOAudioStreamGetFormat                               = 29,
        kTPIOAudioStream_SetFormatAction                        = 30,
        kTPIOAudioStreamSetFormatAction                         = 31,
        kTPIOAudioStreamHardwareFormatChanged                   = 32,
        kTPIOAudioStreamClearAvailableFormats                   = 33,
        kTPIOAudioStreamValidateFormats                         = 34,
        kTPIOAudioStreamGetStartingChannelID                    = 35,
        kTPIOAudioStreamGetMaxNumChannels                       = 36,
        kTPIOAudioStreamSetStartingChannelNumber                = 37,
        kTPIOAudioStreamUpdateNumClients                        = 38,
        kTPIOAudioStreamAddClient                               = 39,
        kTPIOAudioStreamRemoveClient                            = 40,
        kTPIOAudioStreamGetNumClients                           = 41,
        kTPIOAudioStreamDumpList                                = 42,
        kTPIOAudioStreamValidateList                            = 43,
        kTPIOAudioStreamReadInputSamples                        = 44,
        kTPIOAudioStreamProcessOutputSamples                    = 45,
        kTPIOAudioStreamResetClipInfo                           = 46,
        kTPIOAudioStreamClipIfNecessary                         = 47,
        kTPIOAudioStreamClipOutputSamples                       = 48,
        kTPIOAudioStreamLockStreamForIO                         = 49,
        kTPIOAudioStreamUnlockStreamForIO                       = 50,
        kTPIOAudioStreamSetStreamAvailable                      = 51,
        kTPIOAudioStreamGetStreamAvailable                      = 52,
        kTPIOAudioStreamAddDefaultAudioControl                  = 53,
        kTPIOAudioStreamRemoveDefaultAudioControls              = 54
    };
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioToggleControl codes (max 256)
    enum
    {
        kTPIOAudioToggleControlCreatePassThroughMuteControl     = 0,
        kTPIOAudioToggleControlCreate                           = 1,
        kTPIOAudioToggleControlCreateMuteControl                = 2,
        kTPIOAudioToggleControlInit                             = 3
    };
    
    // These are the Tracepoint Codes (8 bits) that are under each audio group
    // kAudioTIOAudioTimeIntervalFilter codes (max 256)
    enum
    {
        kTPIOAudioTimeIntervalFilterInitFilter                  = 0,
        kTPIOAudioTimeIntervalFilterFree                        = 1,
        kTPIOAudioTimeIntervalFilterReInitialiseFilter          = 2,
        kTPIOAudioTimeIntervalFilterNewTimePosition             = 3,
        kTPIOAudioTimeIntervalFilterGetMultiIntervalTime        = 4,
        kTPIOAudioTimeIntervalFilterIIRInitFilter               = 5,
        kTPIOAudioTimeIntervalFilterIIRCalculateNewTimePosition = 6,
        kTPIOAudioTimeIntervalFilterIIR                         = 7,
        kTPIOAudioTimeIntervalFilterFIRInitFilter               = 8,
        kTPIOAudioTimeIntervalFilterFIRSetNewFilter             = 9,
        kTPIOAudioTimeIntervalFilterFIRReInitialiseFilter       = 10,
        kTPIOAudioTimeIntervalFilterFIRFree                     = 11,
        kTPIOAudioTimeIntervalFilterFIRCalculateNewTimePosition = 12,
        kTPIOAudioTimeIntervalFilterFIR                         = 13
    };
    
    

#ifdef KERNEL
    
#include <IOKit/IOTimeStamp.h>
    
#define AudioTrace(AudioClass, code, a, b, c, d) {									\
if (__builtin_expect((gAudioStackDebugFlags & kAudioEnableTracePointsMask), 0)) {	\
    IOTimeStampConstant( AUDIO_TRACE(AudioClass, code), a, b, c, d );               \
    }                                                                               \
}
    
#define AudioTrace_Start(AudioClass, code, a, b, c, d) {							\
if (__builtin_expect((gAudioStackDebugFlags & kAudioEnableTracePointsMask), 0)) {	\
    IOTimeStampStartConstant( AUDIO_TRACE(AudioClass, code), a, b, c, d );          \
    }                                                                               \
}
    
#define AudioTrace_End(AudioClass, code, a, b, c, d) {								\
    if (__builtin_expect((gAudioStackDebugFlags & kAudioEnableTracePointsMask), 0)) {\
    IOTimeStampEndConstant( AUDIO_TRACE(AudioClass, code), a, b, c, d );            \
    }																				\
}
    
#endif
    
#ifdef __cplusplus
}
#endif


#endif
