#include "AudioHardwareObjectInterface.h"
#include "AudioHardwareConstants.h"

OSDefineMetaClassAndAbstractStructors(AudioHardwareObjectInterface, IOService);

// --------------------------------------------------------------------------
//	[3435307]	Manage the mute state at the highest level.  Always updates
//	the hardare to the mute state.  If mute goes inactive then flush out
//	the volume controls.
IOReturn AudioHardwareObjectInterface::setMute(bool mutestate)
{
	IOReturn		result = kIOReturnSuccess;
	IOReturn		tempResult;

	if ( hasAnalogMute() ) {
		tempResult = setMute ( mutestate, kAnalogAudioSelector );			
		if ( kIOReturnSuccess != tempResult ) {
			result = tempResult;
		} else {
			mAnalogMuteState = mutestate;
		}
	}
	if ( hasAnalogMute() ) {
		tempResult = setMute ( mutestate, kDigitalAudioSelector );			
		if ( kIOReturnSuccess != tempResult ) {
			result = tempResult;
		} else {
			mDigitalMuteState = mutestate;
		}
	}
	return result;
}


// --------------------------------------------------------------------------
//	[3435307]	Manage the volume level at the highest level.
//	Behavior for setting the volume is to save the volume setting and only
//	apply the volume to the hardware if the hardware is not muted.  If the
//	hardware is muted then the volume is not applied since the volume may
//	override the mute setting.
bool AudioHardwareObjectInterface::setVolume(UInt32 leftVolume, UInt32 rightVolume)
{
	bool		result = true;
	
	mVolLeft = leftVolume;
	mVolRight = rightVolume;
	if ( !mAnalogMuteState ) {		//	assume that mute & volume exist only on analog codec
		result = setCodecVolume ( mVolLeft, mVolRight );
	}
	return result;
}

// --------------------------------------------------------------------------
//	[3435307]	Manage the mute state at the highest level.
//	If the mute is set then apply the mute to the hardware.  If the mute is
//	unset then apply the mute to the hardware and then set the hardware
//	volume setting to the volume level.
IOReturn AudioHardwareObjectInterface::setMute (bool muteState, UInt32 streamType) {
	IOReturn		result = kIOReturnSuccess;
	
	switch ( streamType ) {
		case kAnalogAudioSelector:		
			mAnalogMuteState = muteState;
			setCodecMute ( muteState, streamType );
			if ( !mAnalogMuteState ) {
				setCodecVolume ( mVolLeft, mVolRight );
			}
			break;
		case kDigitalAudioSelector:	
			mDigitalMuteState = muteState;		
			setCodecMute ( muteState, streamType );
			break;
		default:						
			result = kIOReturnError;			
			break;
	}
	return result;
}
