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
#define NUM_ISUB_FRAME_LISTS_TO_QUEUE	2

class AppleUSBAudioLevelControl;
class AppleUSBAudioMuteControl;

class AppleiSubEngine : public IOService {
    OSDeclareDefaultStructors (AppleiSubEngine);

protected:
	IOUSBInterface *			streamInterface;
	IOUSBInterface *			controlInterface;
	IOUSBIsocFrame				theFrames[NUM_ISUB_FRAMES_PER_LIST * NUM_ISUB_FRAME_LISTS_TO_QUEUE];
	IOUSBIsocCompletion			usbCompletion[NUM_ISUB_FRAME_LISTS];
	IOUSBPipe *					thePipe;
    IORecursiveLock *			interfaceLock;
	IOAudioEngine *				audioEngine;
    AppleUSBAudioLevelControl *	leftVolumeControl;
    AppleUSBAudioLevelControl *	rightVolumeControl;
    AppleUSBAudioMuteControl *	muteControl;
	IOMemoryDescriptor *		soundBuffer[NUM_ISUB_FRAME_LISTS];
	IOMemoryDescriptor *		sampleBufferDescriptor;
	volatile AbsoluteTime		lastLoopTime;
	UInt64						theFirstFrame;
	volatile UInt32				loopCount;
	void *						sampleBuffer;
	UInt32						bufferSize;
	UInt32						currentByteOffset;
	UInt32						frameListSize;
	UInt32						bytesFromStartOfBuffer;
	UInt16						currentFrameList;
	UInt8						shouldStop;
	UInt8						ourInterfaceNumber;
	UInt8						alternateInterfaceID;
	Boolean						iSubRunning;
	Boolean						iSubUSBRunning;

public:
	virtual void				close (IOService * forClient, IOOptionBits options = 0);
	virtual	bool				finalize (IOOptionBits options);
	virtual	void				free (void);
	virtual	bool				handleOpen (IOService * forClient, IOOptionBits options, void * arg);
	virtual	bool				init (OSDictionary * properties);
	virtual	IOReturn			message (UInt32 type, IOService * provider, void * arg);
	virtual	bool				start (IOService * provider);
	virtual	void				stop (IOService * provider);
	virtual	bool				terminate (IOOptionBits options = 0);

	static	IOReturn			deviceRequest (IOUSBDevRequest * request, AppleiSubEngine * self, IOUSBCompletion * completion = 0);
	virtual	UInt32				GetCurrentByteCount (void);
	virtual	UInt16				GetCurrentFrameList (void);
	virtual	UInt32				GetCurrentLoopCount (void);
	virtual	IOMemoryDescriptor *	GetSampleBuffer (void);
	virtual	volatile AbsoluteTime *	GetLoopTime (void);
	virtual	IOReturn			StartiSub (void);
	virtual	IOReturn			StopiSub (void);
	static	void				WriteHandler (AppleiSubEngine * self, UInt32 * buffer, IOReturn result, IOUSBIsocFrame * pFrames);

private:
	virtual	UInt32				CalculateNumSamplesPerBuffer (UInt32 sampleRate, UInt32 theNumFramesPerList, UInt32 theNumFrameLists = 1);
	virtual	void				CalculateSamplesPerFrame (UInt32 sampleRate, UInt16 * averageSamplesPerFrame, UInt16 * additionalSampleFrameFreq);
	virtual	IOReturn			PrepareFrameLists (UInt32 frameListSize);
	virtual	IOReturn			WriteFrameList (UInt32 frameListNum);
};

#endif