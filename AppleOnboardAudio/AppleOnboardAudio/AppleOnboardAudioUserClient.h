/*
 *  AppleOnboardAudioUserClient.h
 *  AppleOnboardAudio
 *
 *  Created by Aram Lindahl on Tue Apr 15 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include <IOKit/IOUserClient.h>

#ifndef __APPLEONBOARDAUDIOUSERCLIENT__
#define	__APPLEONBOARDAUDIOUSERCLIENT__

class AppleOnboardAudio;

// Structure Definitions:
//

typedef enum HardwarePluginType {
	kCodec_Unknown = 0,
	kCodec_TAS3004,
	kCodec_CS8406,
	kCodec_CS8420,
	kCodec_CS8416
} ;
//
// HardwarePlugin State 
//	If this structure changes then please apply the same changes to the DiagnosticSupport sources.
//
typedef struct {
	HardwarePluginType		hardwarePluginType;
	UInt32					registerCacheSize;
	UInt8					registerCache[512];
	UInt32					recoveryRequest;
} HardwarePluginDescriptor;
typedef HardwarePluginDescriptor * HardwarePluginDescriptorPtr;

//===========================================================================================================================
//	AppleOnboardAudioUserClient
//===========================================================================================================================

const UInt32 		kUserClientStateStructSize = 4096;

class AppleOnboardAudioUserClient : public IOUserClient
{
    OSDeclareDefaultStructors( AppleOnboardAudioUserClient )
	
	public:
	
		static const IOExternalMethod		sMethods[];
		static const IOItemCount			sMethodCount;
		//	WARNING:	The following enumerations must maintain the current order.  New
		//				enumerations may be added to the end of the list but must not
		//				be inserted into the center of the list.  
		enum
		{
			kAOAUserClientGetStateIndex	 		=	0,		// returns state for selector																
			kAOAUserClientSetStateIndex,					// writes data to gpio
			kAOAUserClientGetCurrentSampleFrame				// returns TRUE if gpio is active high													
		};

		typedef enum AOAUserClientSelection_t
		{
			kPlatformSelector			 		=	0, 		// FCR1, FCR3, GPIO's and I2S data																
			kHardwarePluginSelector,						// codec state from any codec specificed by arg2
			kDMASelector,									// DMA format and status
			kSoftwareProcessingSelector,					// Software processing state 
			kAppleOnboardAudioSelector,						// Target AppleOnboardAudio (behavior)
			kTransportInterfaceSelector,					// Target Transport Interface instance (sample rate, bit depth, clock source)
			kRealTimeCPUUsage
		} AOAUserClientSelection;

//		static const UInt32 kUserClientStateStructSize;

	protected:
		
		AppleOnboardAudio *					mDriver;
		task_t								mClientTask;
		
	public:
		
		static AppleOnboardAudioUserClient * Create( AppleOnboardAudio *inDriver, task_t task );
		
		// Creation/Deletion
		virtual bool						initWithDriver( AppleOnboardAudio *inDriver, task_t task );
		virtual void						free();
		
		// Public API's
		virtual IOReturn					getState ( UInt32 selector, UInt32 target, void * outStateStructure );
		virtual IOReturn					setState ( UInt32 selector, UInt32 target, void * inStateStructure );
		virtual IOReturn					getCurrentSampleFrame (UInt32 * outCurrentSampleFrame);

	protected:
		
		// IOUserClient overrides
		virtual IOReturn					clientClose();
		virtual IOReturn					clientDied();
		virtual	IOExternalMethod *			getTargetAndMethodForIndex( IOService **outTarget, UInt32 inIndex );

};


#endif /*	__APPLEONBOARDAUDIOUSERCLIENT__		*/
