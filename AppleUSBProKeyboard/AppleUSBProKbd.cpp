/*
	File:		AppleUSBProKbd.c
	Contains:	Driver for second Apple Pro USB Keyboard interface (multimedia keys)
	Version:	1.8.1
	Copyright:	й 2000-2001 by Apple, all rights reserved.

	File Ownership:

		DRI:				Jason Giles
		Other Contact:		Bob Bradley
		Technology:			Apple Pro Keyboard (USB)

	Writers:

		(JG)	Jason Giles

	Change History (most recent first):

		 <4>	03/13/01	JG		Don't auto-repeat on the mute or eject keys. Fix incorrect plist
									info (matching to proper version of USB, made entries that
									somehow became reals into integers).
		 <3>	01/02/00	JG		Override eventFlags() function, and pass back the modifier
									states of all keyboards when called. This way, we can propagate
									the modifiers states for all HID devices, instead of clearing
									them when we get called. Very handy for dealing with special
									key combos. See IOHIDSystem in xnu for a list of used key combos.
         <2>	11/02/00	JG		Inherit from IOHIKeyboard for autorepeat. Add keymap so that
									we can post the proper events into the event stream when
									the buttons are hit. Eject will be handled by someone higher
									up, watching for that key event. Modify implementation to
									conform to new USB family changes.
		 <1>	 7/25/00	JG		First checked in; audio feedback is currently disabled
									so behavior matches the vul up/down/mute keys on Powerbooks.
									The eject key does not currently do anything, and won't
									until post public beta.
*/

#include "AppleUSBProKbd.h"

//#include <libkern/OSByteOrder.h>

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>

#include <IOKit/hidsystem/ev_keymap.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIDShared.h>
//#include <IOKit/hidsystem/IOHIDSystem.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>


//====================================================================================================
// Defines
//====================================================================================================

#define super 			IOHIKeyboard
#define DEBUGGING_LEVEL 0
#define kMaxValues		32
#define kVolumeUp		0x06
#define kVolumeDown		0x07
#define kVolumeMute		0x08
#define kEject			0x10

OSDefineMetaClassAndStructors( AppleUSBProKbd, IOHIKeyboard )

#pragma mark -
#pragma mark еее inherited еее

bool
AppleUSBProKbd::init(OSDictionary *properties)
{
  if (!super::init(properties))  return false;
  
    mSoundUpIsPressed = false;
    mSoundDownIsPressed = false;
    mOutstandingIO = 0;
    mNeedToClose = false;

    return true;
}



//====================================================================================================
// start
//====================================================================================================

bool AppleUSBProKbd::start( IOService * provider )
{
    IOReturn			err;
    IOWorkLoop			*wl;
		
	
    USBLog(3, "%s[%p]::start - beginning - retain count = %d", getName(), this, getRetainCount());
    mInterface = OSDynamicCast(IOUSBInterface, provider);
    if (!mInterface)
	return false;
		    
    if( mInterface->open( this ) == false )
    {
	USBError(1, "%s[%p]::start - unable to open provider. returning false", getName(), this);
	return false;
    }
    
    do {
        mGate = IOCommandGate::commandGate(this);

        if(!mGate)
        {
            USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
            break;
        }

	wl = getWorkLoop();
	if (!wl)
	{
            USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
            break;
	}
	
        if (wl->addEventSource(mGate) != kIOReturnSuccess)
        {
            USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
            break;
        }

	IOUSBFindEndpointRequest	endpointRequest;
	endpointRequest.type 		= kUSBInterrupt;
	endpointRequest.direction	= kUSBIn;
	mInterruptPipe = mInterface->FindNextPipe( NULL, &endpointRequest );
	
	if( !mInterruptPipe )
	{
            USBError(1, "%s[%p]::start - unable to get interrupt pipe", getName(), this);
	    break;
	}
	
	// Check for all the usages we expect to find on this device. If we don't find what we're
	// looking for, we're gonna bail.
	
	if( VerifyNewDevice() == false )
	{
            USBError(1, "%s[%p]::start - VerifyNewDevice was not successful. Ignoring this device", getName(), this);
	    break;
	}

	// Setup the read buffer.
	
	mMaxPacketSize = endpointRequest.maxPacketSize;
	mReadDataBuffer = IOBufferMemoryDescriptor::withCapacity( mMaxPacketSize, kIODirectionIn );
		
	mCompletionRoutine.target = (void *) this;
	mCompletionRoutine.action = (IOUSBCompletionAction) AppleUSBProKbd::InterruptReadHandlerEntry;
	mCompletionRoutine.parameter = (void *) 0;  // not used

	mReadDataBuffer->setLength( mMaxPacketSize );

	// The way to set us up to recieve reads is to call it directly. Each time our read handler
	// is called, we'll have to do another to make sure we get the next read.

        IncrementOutstandingIO();
	if( err = mInterruptPipe->Read( mReadDataBuffer, &mCompletionRoutine ) )
	{
	    USBError(1, "%s[%p]::start - err (%x) in interrupt read, retain count %d after release", getName(), this, err, getRetainCount());
            DecrementOutstandingIO();
	    break;
	}
	
        USBError(1, "%s[%p]::start AppleUSBProKeyboard @ %d (0x%x)", getName(), this, mInterface->GetDevice()->GetAddress(), strtol(mInterface->GetDevice()->getLocation(), (char **)NULL, 16));

	// OK- so this is not totally kosher in the IOKit world. You are supposed to call super::start near the BEGINNING
	// of your own start method. However, the IOHIKeyboard::start method invokes registerService, which we don't want to
	// do if we get any error up to this point. So we wait and call IOHIKeyboard::start here.
	if( !super::start(mInterface))
	{
	    USBError(1, "%s[%p]::start - unable to start superclass. returning false", getName(), this);
	    break;	// error
	}
	    
        return true;

    } while (false);
	
    USBLog(3, "%s[%p]::start aborting.  err = 0x%x", getName(), this, err);

    if( mPreparsedReportDescriptorData ) 
        HIDCloseReportDescriptor( mPreparsedReportDescriptorData );
	
    if ( mInterruptPipe )
    {
	mInterruptPipe->Abort();
	mInterruptPipe = NULL;
    }

    // stop will clean up everything else
    stop( provider );
    provider->close( this );
	
    return false;
}

//====================================================================================================
// stop
//====================================================================================================

void AppleUSBProKbd::stop( IOService * provider )
{	
    if( mPreparsedReportDescriptorData ) 
        HIDCloseReportDescriptor( mPreparsedReportDescriptorData );
		
    if (mGate)
    {
	IOWorkLoop	*wl = getWorkLoop();
	if (wl)
	    wl->removeEventSource(mGate);
	mGate->release();
	mGate = NULL;
    }
    
    super::stop( provider );
}



//====================================================================================================
// eventFlags - IOHIKeyboard override. This is necessary because we will need to return the state
//				of the modifier keys on attached keyboards. If we don't, then we the HIDSystem gets
//				the event, it will look like all modifiers are off.
//====================================================================================================

unsigned AppleUSBProKbd::eventFlags()
{
	//IOLog( "***AppleUSBProKeyboard - eventFlags called, return value = %x\n", mEventFlags );
    return( mEventFlags );
}

//====================================================================================================
// alphaLock  - IOHIKeyboard override. This is necessary because we will need to return the state
//				of the caps lock keys on attached keyboards. If we don't, then we the HIDSystem gets
//				the event, it will look like caps lock keys are off.
//====================================================================================================

bool AppleUSBProKbd::alphaLock()
{
	//IOLog( "***AppleUSBProKeyboard - alphaLock called, return value = %d\n", mCapsLockOn );
    return( mCapsLockOn );
}

//====================================================================================================
// defaultKeymapOfLength - IOHIKeyboard override
// This allows us to associate the scancodes we choose with the special
// keys we are interested in posting later. This gives us auto-repeats for free. Kewl.
//====================================================================================================

const unsigned char * AppleUSBProKbd::defaultKeymapOfLength( UInt32 * length )
{
    static const unsigned char AppleProKeyboardKeyMap[] =
    {
		// The first 16 bits are always read first, to determine if the rest of
        // the keymap is in shorts (16 bits) or bytes (8 bits). If the first 16 bits
        // equals 0, data is in bytes; if first 16 bits equal 1, data is in shorts.
        
        0x00,0x00,		// data is in bytes

        // The next value is the number of modifier keys. We have none in our driver.

        0x00,
        
        // The next value is number of key definitions. We have none in our driver.
        
        0x00,
        
        // The next value is number of of sequence definitions there are. We have none.
        
        0x00,
        
        // The next value is the number of special keys. We use these.
        
        0x04,
        
		// Special Key	  		SCANCODE
        //-----------------------------------------------------------
        NX_KEYTYPE_SOUND_UP, 	kVolumeUp,
        NX_KEYTYPE_SOUND_DOWN, 	kVolumeDown,
        NX_KEYTYPE_MUTE, 		kVolumeMute,
        NX_KEYTYPE_EJECT, 		kEject
    };
    
 
    if( length ) *length = sizeof( AppleProKeyboardKeyMap );
    
    return( AppleProKeyboardKeyMap );
}

#pragma mark -
#pragma mark еее Implementation еее


//====================================================================================================
// VerifyNewDevice
// Check that the device we were called to match has the proper usage stuff in it's report descriptor.
//====================================================================================================

bool AppleUSBProKbd::VerifyNewDevice ()
{
	IOReturn			err;
  	bool 				success 			= true;
    UInt16				size 				= 0;
    OSStatus			result;

    IOUSBDevRequest		devReq;
    IOUSBHIDDescriptor	hidDescriptor;
    HIDButtonCaps		buttonCaps[kMaxValues];
    UInt32				buttonCapsSize 		= kMaxValues;
    UInt8 *				reportDescriptor	= NULL;
	
    do
    {
    	// Set up the device request record.
        devReq.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);      
        devReq.bRequest = kUSBRqGetDescriptor;
        devReq.wValue	= (0x20 | kUSBHIDDesc) << 8;
        devReq.wIndex	= mInterface->GetInterfaceNumber();
        devReq.wLength	= sizeof(IOUSBHIDDescriptor);
        devReq.pData = &hidDescriptor;

        // Do a device request to get the HID Descriptor.
		
        err = mInterface->DeviceRequest(&devReq);
        if( err )
        {
            IOLog ("****AppleUSBProKbd: error making deviceRequest on interface.  err=0x%x\n", err);
            success = false;
            break;
        }

		// Allocate space to get the HID report descriptor.
		
        size = (hidDescriptor.hidDescriptorLengthHi * 256) + hidDescriptor.hidDescriptorLengthLo;
        reportDescriptor = (UInt8*) IOMalloc( size );

        devReq.wValue = ((0x20 | kUSBReportDesc) << 8);
        devReq.wLength = size;
        devReq.pData = reportDescriptor;

		// Get the HID report descriptor.
		
        err = mInterface->DeviceRequest( &devReq );
        if( err )
        {
			IOLog ("****AppleUSBProKbd: deviceRequest failed.  err=0x%x\n", err);
			success = false;
        }

    
		// Open the HID report descriptor, so we get a reference to the parsed data. Break out
		// if we fail here, since there's no point in continuing.
		
		result = HIDOpenReportDescriptor( reportDescriptor, size, &mPreparsedReportDescriptorData, 0 );
        if( result != noErr )
        {
			IOLog ("****AppleUSBProKbd: error opening HID report descriptor.  err=0x%lx\n", result );
			success = false;
        }

		// All we're doing here is verifying that we have the correct buttons (keys) on the keyboard.
		// If we don't, then we'll return false because it's not a device we're interested in loading for.
		
		// Check for the consumer page, eject button.
		
		buttonCapsSize = kMaxValues;
		result = HIDGetSpecificButtonCaps(	kHIDInputReport,
											kHIDPage_Consumer,
											0,
											kHIDUsage_Csmr_Eject,
											buttonCaps,
											&buttonCapsSize,
											mPreparsedReportDescriptorData );
		if( (result == noErr) && (buttonCapsSize > 0) )
		{
			// IOLog( "****AppleUSBProKbd::ParseHIDDescriptor->HIDGetSpecifcButtonCaps returned buttonCapsSize = %d", (int) buttonCapsSize );
		}
		else
		{
			IOLog( "****AppleUSBProKbd::ParseHIDDescriptor - HIDGetSpecifcButtonCaps returned an error = %d\n", (int) result );
			success = false;
		}

		// Check for the consumer page, volume up button.

		buttonCapsSize = kMaxValues;
		result = HIDGetSpecificButtonCaps(	kHIDInputReport,
											kHIDPage_Consumer,
											0,
											kHIDUsage_Csmr_VolumeIncrement,
											buttonCaps,
											&buttonCapsSize,
											mPreparsedReportDescriptorData );
		if( (result == noErr) && (buttonCapsSize > 0) )
		{
			// IOLog( "****AppleUSBProKbd::ParseHIDDescriptor->HIDGetSpecifcButtonCaps returned numValueCaps = %d", (int) buttonCapsSize );
		}
		else
		{
			IOLog( "****AppleUSBProKbd::ParseHIDDescriptor - HIDGetSpecifcButtonCaps returned an error = %d\n", (int) result );
			success = false;
		}

		// Check for the consumer page, volume down button.

		buttonCapsSize = kMaxValues;
		result = HIDGetSpecificButtonCaps(	kHIDInputReport,
											kHIDPage_Consumer,
											0,
											kHIDUsage_Csmr_VolumeDecrement,
											buttonCaps,
											&buttonCapsSize,
											mPreparsedReportDescriptorData );
		if( (result == noErr) && (buttonCapsSize > 0) )
		{
			// IOLog( "****AppleUSBProKbd::ParseHIDDescriptor->HIDGetSpecifcButtonCaps returned numValueCaps = %d", (int) buttonCapsSize );
		}
		else
		{
			IOLog( "****AppleUSBProKbd::ParseHIDDescriptor - HIDGetSpecifcButtonCaps returned an error = %d\n", (int) result );
			success = false;
		}

		// Check for the consumer page, volume mute button.
		
		buttonCapsSize = kMaxValues;
		result = HIDGetSpecificButtonCaps(	kHIDInputReport,
											kHIDPage_Consumer,
											0,
											kHIDUsage_Csmr_Mute,
											buttonCaps,
											&buttonCapsSize,
											mPreparsedReportDescriptorData );
		if( (result == noErr) && (buttonCapsSize > 0) )
		{
			// IOLog( "****AppleUSBProKbd::ParseHIDDescriptor->HIDGetSpecifcButtonCaps returned numValueCaps = %d", (int) buttonCapsSize );
		}
		else
		{
			IOLog( "****AppleUSBProKbd::ParseHIDDescriptor- HIDGetSpecifcButtonCaps returned an error = %d\n", (int) result );
			success = false;
		}
	}
	while ( false );
	
	// We're done, get rid of the report descriptor.
	
	if( reportDescriptor )
	{
		IOFree ( reportDescriptor, size );
	}
	
	return ( success );
}

//====================================================================================================
// ReadHandler
//====================================================================================================

void 
AppleUSBProKbd::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
    AppleUSBProKbd *	me = OSDynamicCast(AppleUSBProKbd, target);

    if (!me)
        return;
    
    me->InterruptReadHandler(status, bufferSizeRemaining);
    me->DecrementOutstandingIO();
}



void
AppleUSBProKbd::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining)
{	 
    bool		queueAnother = true;
    IOReturn		err = kIOReturnSuccess;

    switch ( status )
    {
        case kIOReturnOverrun:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // Not sure what to do with this error.  It means more data
            // came back than the size of a descriptor.  Hmmm.  For now
            // just ignore it and assume the data that did come back is
            // useful.
	    // INTENTIONAL FALL THROUGH
        case kIOReturnSuccess:
	    // We got some good stuff, so jump right to our special
	    // button handler.
	    HandleSpecialButtons( (UInt8*) mReadDataBuffer->getBytesNoCopy(),((UInt32)mMaxPacketSize - bufferSizeRemaining) );
            break;

        case kIOReturnNotResponding:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnNotResponding error", getName(), this);
            if ( isInactive() )
            {
                  queueAnother = false;
            }
	    // if we are not yet inactive, then we should just try again. if we are really disconnected, then
	    // we will eventually be terminated and isInactive will return true
	    break;

 	case kIOReturnAborted:
	    // This generally means that we are done, because we were unplugged, but not always
            if (isInactive())
	    {
                USBLog(3,"%s[%p]::InterruptReadHandler Read aborted. We are terminating", getName(), this);
		queueAnother = false;
	    }
	    else
            {
                USBLog(3,"%s[%p]::InterruptReadHandler Read aborted. Don't know why. Trying again", getName(), this);
            }
	    break;

        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            USBLog(3, "%s[%p]::InterruptReadHandler error (0x%x) reading interrupt pipe", getName(), this, status);
            break;
    }

    if (queueAnother)
    {
	// Reset the buffer - i doubt this is really necessary
	mReadDataBuffer->setLength( mMaxPacketSize );

	// Queue up another one before we leave.
        IncrementOutstandingIO();
    	err = mInterruptPipe->Read( mReadDataBuffer, &mCompletionRoutine );
        if ( err != kIOReturnSuccess)
        {
            // This is bad.  We probably shouldn't continue on from here.
            USBError(1, "%s[%p]::InterruptReadHandler -  immediate error 0x%x queueing read\n", getName(), this, err);
            DecrementOutstandingIO();
        }
    }
}

//====================================================================================================
// HandleSpecialButtons
//====================================================================================================

void AppleUSBProKbd::HandleSpecialButtons(	UInt8 *	inBufferData,
											UInt32	bufferSize		)
{
	HIDUsageAndPage		usageList[kMaxValues];
	OSStatus			err;
    UInt32				usageListSize	= kMaxValues;
	UInt32 				reportIndex		= 0;
	AbsoluteTime		now;
 	Boolean				soundUpIsPressed		= FALSE;
	Boolean				soundDownIsPressed		= FALSE;
	
	// Get our button state from the report.
	
	err = HIDGetButtons( kHIDInputReport, 0, usageList, &usageListSize, mPreparsedReportDescriptorData,
						 inBufferData, bufferSize );
	if( err )
	{
            USBError(1, "%s[%p]::HandleSpecialButtons -  HIDGetButtons failed (0x%x)", getName(), this, err);
            return;
	}
		
	// Record current time for the keypress posting.
	
	clock_get_uptime( &now );

	// Get modifier states for all attached keyboards. This way, when we are queried as to
	// what the state of event flags are, we can tell them and be correct about it.

	FindKeyboardsAndGetModifiers();
	
	// Iterate through the entire usage list and see what we've got.
	
	for( reportIndex = 0; reportIndex < usageListSize; reportIndex++ )
	{
		// Is this part on the consumer page? If not, we don't care!
		
		if( usageList[reportIndex].usagePage == kHIDPage_Consumer )
		{
			// What usage is it?
		
			switch( usageList[reportIndex].usage )
			{
				case( kHIDUsage_Csmr_VolumeIncrement ):
				{
					soundUpIsPressed = TRUE;
					break;
				}
				case( kHIDUsage_Csmr_VolumeDecrement ):
				{
					soundDownIsPressed = TRUE;
					break;
				}
				case( kHIDUsage_Csmr_Mute ):
				{
					// Post key down and key up events. We don't want auto-repeat happening here.
					
					dispatchKeyboardEvent( kVolumeMute, TRUE, now );
					dispatchKeyboardEvent( kVolumeMute, FALSE, now );
					break;
				}
				case( kHIDUsage_Csmr_Eject ):
				{
					// Post key down and key up events. We don't want auto-repeat happening here.

					dispatchKeyboardEvent( kEject, TRUE, now );
					dispatchKeyboardEvent( kEject, FALSE, now );
					break;
				}
				default:
                                    USBError(1, "%s[%p]::HandleSpecialButtons -  no usage found for report. Usage = %d", getName(), this, usageList[reportIndex].usagePage);
					break;
			}
		}
	}

	// Check and see if the states have changed since last report, if so, we'll notify whoever
	// cares by posting the appropriate key and keystates.

	if( soundUpIsPressed != mSoundUpIsPressed )
	{
		// Sound up state has changed.
		dispatchKeyboardEvent( kVolumeUp, soundUpIsPressed, now );
	}
	
	if( soundDownIsPressed != mSoundDownIsPressed )
	{
		// Sound down state has changed.
		dispatchKeyboardEvent( kVolumeDown, soundDownIsPressed, now );
	}
	
	// Save states for our next report.
	
	mSoundUpIsPressed	= soundUpIsPressed;
	mSoundDownIsPressed	= soundDownIsPressed;
}

//====================================================================================================
// FindKeyboardsAndGetModifiers
//====================================================================================================

UInt32 AppleUSBProKbd::FindKeyboardsAndGetModifiers()
{
	OSIterator		*iterator 			= NULL;
	OSDictionary	*matchingDictionary = NULL;
	IOHIKeyboard	*device 			= NULL;
	Boolean 		value				= false;
	
	
	mEventFlags = 0;
	mCapsLockOn = FALSE;
	
	// Get matching dictionary.
	
	matchingDictionary = IOService::serviceMatching( "IOHIKeyboard" );
	if( !matchingDictionary )
	{
                USBError(1, "%s[%p]::FindKeyboardsAndGetModifiers -  could not get a matching dictionary", getName(), this);
		goto exit;
	}
	
	// Get an iterator for the IOHIKeyboard devices.
	
	iterator = IOService::getMatchingServices( matchingDictionary );
	if( !iterator )
	{
                USBError(1, "%s[%p]::FindKeyboardsAndGetModifiers -  getMatchingServices failed", getName(), this);
		goto exit;
	}
	
	// User iterator to find devices and eject.
	//
	while( (device = (IOHIKeyboard*) iterator->getNextObject()) )
	{		
		value = false;
		
		// Save the caps lock state. If more than one keyboard has it down, that's fine -- we
		// just want to know if ANY keyboards have the key down.
		//
		if( device->alphaLock() )
		{
			mCapsLockOn = TRUE;
		}

		// OR in the flags, so we get a combined IOHIKeyboard device flags state. That
		// way, if the user is pressing command on one keyboard, shift on another, and
		// then hits an eject key, we'll get both modifiers.
		//
		mEventFlags |= device->eventFlags();
	}
	
exit:
	
	if( matchingDictionary ) matchingDictionary->release();
	if( iterator ) iterator->release();
	
	return( mEventFlags );
}


//====================================================================================================
// message
//====================================================================================================

IOReturn AppleUSBProKbd::message( UInt32 type, IOService * provider,  void * argument = 0 )
{
    switch ( type )
    {
	case kIOMessageServiceIsTerminated:
	    USBLog(3, "%s[%p]::message - kIOMessageServiceIsTerminated - ignoring now", getName(), this);
	    break;

	case kIOMessageServiceIsSuspended:
	case kIOMessageServiceIsResumed:
	case kIOMessageServiceIsRequestingClose:
	case kIOMessageServiceWasClosed: 
	case kIOMessageServiceBusyStateChange:
	default:
            break;
    }
    
    return kIOReturnSuccess;
}



bool
AppleUSBProKbd::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    if (mReadDataBuffer) 
    {
	mReadDataBuffer->release();
        mReadDataBuffer = NULL;
    }
    if (mInterruptPipe)
    {
	mInterruptPipe->Abort();
	mInterruptPipe = NULL;
    }
    return super::willTerminate(provider, options);
}


bool
AppleUSBProKbd::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(3, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), mOutstandingIO);
    if (!mOutstandingIO)
	mInterface->close(this);
    else
	mNeedToClose = true;
    return super::didTerminate(provider, options, defer);
}


void
AppleUSBProKbd::DecrementOutstandingIO(void)
{
    if (!mGate)
    {
	if (!--mOutstandingIO && mNeedToClose)
	{
	    USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), mOutstandingIO);
	    mInterface->close(this);
	}
	return;
    }
    mGate->runAction(ChangeOutstandingIO, (void*)-1);
}


void
AppleUSBProKbd::IncrementOutstandingIO(void)
{
    if (!mGate)
    {
	mOutstandingIO++;
	return;
    }
    mGate->runAction(ChangeOutstandingIO, (void*)1);
}


IOReturn
AppleUSBProKbd::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    AppleUSBProKbd *me = OSDynamicCast(AppleUSBProKbd, target);
    UInt32	direction = (UInt32)param1;
    
    if (!me)
    {
	USBLog(1, "AppleUSBProKbd::ChangeOutstandingIO - invalid target");
	return kIOReturnSuccess;
    }
    switch (direction)
    {
	case 1:
	    me->mOutstandingIO++;
	    break;
	    
	case -1:
	    if (!--me->mOutstandingIO && me->mNeedToClose)
	    {
		USBLog(3, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->mOutstandingIO);
		me->mInterface->close(me);
	    }
	    break;
	    
	default:
	    USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }
    return kIOReturnSuccess;
}


