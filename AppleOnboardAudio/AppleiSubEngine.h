#ifndef __ISUBENGINE__
#define __ISUBENGINE__

#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/audio/IOAudioEngine.h>

#define kMinimumiSubFrameOffset			1

#define NUM_ISUB_FRAME_LISTS			20
#define NUM_ISUB_FRAMES_PER_LIST		10
#define NUM_ISUB_FRAME_LISTS_TO_QUEUE	10

class AppleUSBAudioLevelControl;
class AppleUSBAudioMuteControl;

#include "iSubTypes.h"		

class AppleiSubEngine : public IOService {
    OSDeclareDefaultStructors (AppleiSubEngine);

protected:
	IOUSBInterface *			streamInterface;
	IOUSBInterface *			controlInterface;
	IOUSBIsocFrame				theFrames[NUM_ISUB_FRAMES_PER_LIST * NUM_ISUB_FRAME_LISTS_TO_QUEUE];
	IOUSBIsocCompletion			usbCompletion[NUM_ISUB_FRAME_LISTS];
	IOUSBPipe *					thePipe;
	IOAudioEngine *				audioEngine;
    AppleUSBAudioLevelControl *	iSubVolumeControl;
    AppleUSBAudioMuteControl *	muteControl;
	IOMemoryDescriptor *		soundBuffer[NUM_ISUB_FRAME_LISTS];
	IOMemoryDescriptor *		sampleBufferDescriptor;
	volatile AbsoluteTime		lastLoopTime;
	UInt64						theFirstFrame;
	volatile UInt32				loopCount;
	void *						sampleBuffer;
	IONotifier *				sleepWakeHandlerNotifier;
	UInt32						bufferSize;
	UInt32						currentByteOffset;
	UInt32						frameListSize;
	UInt32						bytesFromStartOfBuffer;
	UInt16						currentFrameList;
	UInt8						numUSBFrameListsNotOutstanding;
	UInt8						ourInterfaceNumber;
	UInt8						alternateInterfaceID;
	Boolean						iSubRunning;
	Boolean						iSubUSBRunning;
	Boolean						streamOpened;
	Boolean						shouldCloseStream;
	Boolean						sleeping;
	Boolean						mNeedToSync;	// aml [3095619]

	// aml 2.28.02 adding protected member to describe iSub configuration
	iSubAudioFormatType			mFormat;

	// aml 2.28.02 - adding sets to protected data, these are protected for now too, no other object types should call these yet
	void						SetAltInterface (const iSubAltInterfaceType inAltInt) {mFormat.altInterface = inAltInt;}
	void						SetNumChannels (const long inNumChannels) {mFormat.numChannels = inNumChannels;}
	void						SetBytesPerSample (const long inBytesPerSample) {mFormat.bytesPerSample = inBytesPerSample;}
	void						SetSampleRate (const long inOutputSampleRate) {mFormat.outputSampleRate = inOutputSampleRate;}

	//
	// protected default constants, aml 2.28.02 
	//
	static const iSubAltInterfaceType 	kDefaultiSubAltInterface = e_iSubAltInterface_16bit_Mono;
	static const UInt32 			kDefaultNumChannels = 1;	// aml 3.4.02 changed to 1 from 2
	static const UInt32 			kDefaultBytesPerSample = 2;
	static const UInt32 			kDefaultOutputSampleRate = 6000;	// aml 3.4.02 changed to 6k from 44.1k	

public:
	virtual void				closeiSub (IOService * forClient);
	virtual bool				openiSub (IOService * forClient);

//	virtual void				close (IOService * forClient, IOOptionBits options = 0);
	virtual	void				free (void);
//	virtual	bool				handleOpen (IOService * forClient, IOOptionBits options, void * arg);
	virtual	bool				init (OSDictionary * properties);
	virtual	bool				start (IOService * provider);
	virtual	bool				terminate (IOOptionBits options = 0);
	virtual bool				willTerminate (IOService * provider, IOOptionBits options);
	virtual IOReturn			setPowerState (unsigned long powerStateOrdinal, IOService *device);

	static	IOReturn			deviceRequest (IOUSBDevRequest * request, AppleiSubEngine * self, IOUSBCompletion * completion = NULL);
	virtual	UInt32				GetCurrentByteCount (void);
	virtual	UInt32				GetCurrentLoopCount (void);
	virtual	IOMemoryDescriptor *	GetSampleBuffer (void);
	virtual	IOReturn			StartiSub (void);
	virtual	IOReturn			StopiSub (void);
	static	void				WriteHandler (void * object, void * buffer, IOReturn result, IOUSBIsocFrame * pFrames);
        
	// aml 2.28.02 - adding accessors to protected data
	virtual	iSubAltInterfaceType		GetAltInterface (void) const {return mFormat.altInterface;}
	virtual	UInt32				GetNumChannels (void) const {return mFormat.numChannels;}
	virtual	UInt32				GetBytesPerSample (void) const {return mFormat.bytesPerSample;}
	virtual	UInt32				GetSampleRate (void) const {return mFormat.outputSampleRate;}

	// aml 3.01.02 - adding accessors to default settings
	virtual	iSubAltInterfaceType		GetDefaultAltInterface (void) const {return kDefaultiSubAltInterface;}
	virtual	UInt32				GetDefaultNumChannels (void) const {return kDefaultNumChannels;}
	virtual	UInt32				GetDefaultBytesPerSample (void) const {return kDefaultBytesPerSample;}
	virtual	UInt32				GetDefaultSampleRate (void) const {return kDefaultOutputSampleRate;}

	// aml [3095619] added two methods
			Boolean				GetNeedToSync() { return mNeedToSync; };
			void				SetNeedToSync(Boolean inNeedToSync) { mNeedToSync = inNeedToSync; };

private:
	virtual	UInt32				CalculateNumSamplesPerBuffer (UInt32 sampleRate, UInt32 theNumFramesPerList, UInt32 theNumFrameLists = 1);
	virtual	void				CalculateSamplesPerFrame (UInt32 sampleRate, UInt16 * averageSamplesPerFrame, UInt16 * additionalSampleFrameFreq);
	virtual	IOReturn			PrepareFrameLists (UInt32 frameListSize);
	virtual	IOReturn			WriteFrameList (UInt32 frameListNum);
};

#endif
