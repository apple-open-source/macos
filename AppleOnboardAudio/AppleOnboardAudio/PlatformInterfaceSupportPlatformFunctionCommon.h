/*
 *  PlatformInterfaceSupportPlatformFunctionCommon.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 22 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include	<IOKit/IOService.h>
#include	<libkern/c++/OSString.h>
#include	"AppleOnboardAudio.h"

#ifndef __PlatformInterfaceSupportPlatformFunctionCommon
#define	__PlatformInterfaceSupportPlatformFunctionCommon

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Enable									"platform-enable"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Disable								"platform-disable"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_ClockEnable							"platform-clock-enable"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_ClockDisable							"platform-clock-disable"	
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Reset									"platform-sw-reset"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Run									"platform-clear-sw-reset"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_CellEnable								"platform-cell-enable"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_CellDisable							"platform-cell-disable"	
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetEnable								"platform-get-enable"	
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetClockEnable							"platform-get-clock-enable"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetReset								"platform-get-sw-reset"
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetCellEnable							"platform-get-cell-enable"	

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetAmpMute							"platform-amp-mute"					
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetAmpMute							"platform-rd-amp-mute"				

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetAudioHwReset						"platform-hw-reset"					
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetAudioHwReset						"platform-rd-hw-reset"				

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetCodecClockMux						"platform-codec-clock-mux"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecClockMux						"platform-rd-clock-mux"				

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableCodecErrorIRQ					"enable-codec-error-irq"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableCodecErrorIRQ					"disable-codec-error-irq"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecErrorIRQ						"platform-codec-error-irq"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterCodecErrorIRQ					"register-codec-error-irq"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterCodecErrorIRQ				"unregister-codec-error-irq"		

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableCodecIRQ						"disable-codec-irq";				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableCodecIRQ						"enable-codec-irq"					
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecIRQ							"platform-codec-irq"				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterCodecIRQ						"register-codec-irq"				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterCodecIRQ					"unregister-codec-irq"				

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetCodecInputDataMux					"platform-codec-input-data-mu"		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecInputDataMux					"get-codec-input-data-mux"			

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetComboInJackType					"platform-combo-in-sense";		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableComboInSense					"disable-combo-in-sense";
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableComboInSense					"enable-combo-in-sense";
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterComboInSense					"register-combo-in-sense";
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterComboInSense				"unregister-combo-in-sense";

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetComboOutJackType					"platform-combo-out-sense";
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableComboOutSense					"disable-combo-out-sense";
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableComboOutSense					"enable-combo-out-sense";
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterComboOutSense					"register-combo-out-sense";
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterComboOutSense				"unregister-combo-out-sense";

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetAudioDigHwReset					"platform-dig-hw-reset"				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetAudioDigHwReset					"get-dig-hw-reset"					

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableDigitalInDetect				"disable-audio-dig-in-det"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableDigitalInDetect					"enable-audio-dig-in-det"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetDigitalInDetect					"platform-audio-dig-in-det"		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterDigitalInDetect				"register-audio-dig-in-det"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterDigitalInDetect				"unregister-audio-dig-in-det"		

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableDigitalOutDetect				"disable-audio-dig-out-detect"		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableDigitalOutDetect				"enable-audio-dig-out-detect"		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetDigitalOutDetect					"platform-audio-dig-out-det"	
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterDigitalOutDetect				"register-audio-dig-out-detect"		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterDigitalOutDetect			"unregister-audio-dig-out-detect"	
	
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableHeadphoneDetect				"disable-headphone-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableHeadphoneDetect					"enable-headphone-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetHeadphoneDetect					"platform-headphone-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterHeadphoneDetect				"register-headphone-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterHeadphoneDetect				"unregister-headphone-detect"		

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetHeadphoneMute						"platform-headphone-mute"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetHeadphoneMute						"platform-rd-headphone-mute"		

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetInternalSpeakerID					"internal-speaker-id"				

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableLineInDetect					"disable-linein-detect"				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableLineInDetect					"enable-linein-detect"				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetLineInDetect						"platform-linein-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterLineInDetect					"register-linein-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterLineInDetect				"unregister-linein-detect"			

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableLineOutDetect					"disable-lineout-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableLineOutDetect					"enable-lineout-detect"				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetLineOutDetect						"platform-lineout-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterLineOutDetect					"register-lineout-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterLineOutDetect				"unregister-lineout-detect"			

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetLineOutMute						"platform-lineout-mute"				
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetLineOutMute						"platform-rd-lineout-mute"			

#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableSpeakerDetect					"disable-audio-spkr-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableSpeakerDetect					"enable-audio-spkr-detect"			
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetSpeakerDetect						"platform-audio-spkr-detect"		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterSpeakerDetect					"register-audio-spkr-detect"		
#define	kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterSpeakerDetect				"unregister-audio-spkr-detect"		



#endif
