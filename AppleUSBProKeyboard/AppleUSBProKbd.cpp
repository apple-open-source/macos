/*
	File:		AppleUSBProKbd.c
	Contains:	Driver for second Apple Pro USB Keyboard interface (multimedia keys)
	Version:	2.0.0d1
	Copyright:	й 2000-2002 by Apple, all rights reserved.

	File Ownership:

		DRI:				Jason Giles
		Other Contact:		Bob Bradley
		Technology:			Apple Pro Keyboard (USB)

	Writers:

		(JG)	Jason Giles

	Change History (most recent first):
		 <5>     3/14/02	AW		Change version number to conform to new standard
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

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOService.h>

#include <IOKit/hidsystem/ev_keymap.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDSystem.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/IOMessage.h>

#include <libkern/OSByteOrder.h>

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

//====================================================================================================
// start
//====================================================================================================

bool AppleUSBProKbd::start( IOService * provider )
{
    IOReturn			err = kIOReturnSuccess;

	// Reset key state flags.

	mSoundUpIsPressed		= FALSE;
	mSoundDownIsPressed		= FALSE;
	
	// To start, call our super's start.
	
        if( !super::start( provider ) ) return( false );
		
	// Remember my interface info.
	
	mInterface			= OSDynamicCast(IOUSBInterface, provider);
        if (!mInterface)
            return false;
	
        // IOLog( "****AppleUSBProKbd::Pro keyboard product ID = %x\n", mInterface->GetDevice()->GetProductID() );
	
	// Open the interface.
	
	if( mInterface->open( this ) == false )
	{
            IOLog("****AppleUSBProKbd::start - interface open failed.\n");
            return false;
        }
        
	fTerminating = FALSE;
	// Let's get the pipe on the interface so we can read from it.
	
	IOUSBFindEndpointRequest	endpointRequest;

	endpointRequest.type 		= kUSBInterrupt;
	endpointRequest.direction	= kUSBIn;

	mInterruptPipe = mInterface->FindNextPipe( NULL, &endpointRequest );
	if( !mInterruptPipe )
	{
    	IOLog( "****AppleUSBProKbd::start - could not get the interrupt pipe.\n" );
         goto failedExit;
	}
	
	// Check for all the usages we expect to find on this device. If we don't find what we're
	// looking for, we're gonna bail.
	
	if( VerifyNewDevice() == false )
	{
       // Error!
       IOLog("****AppleUSBProKbd::start - VerifyNewDevice was not successful. Ignoring this device.\n" );
       
         goto failedExit;
    }

	// Setup the read buffer.
	
	mMaxPacketSize = endpointRequest.maxPacketSize;
	mReadDataBuffer = IOBufferMemoryDescriptor::withCapacity( 8, kIODirectionIn );
		
	mCompletionRoutine.target = (void *) this;
	mCompletionRoutine.action = (IOUSBCompletionAction) AppleUSBProKbd::ReadHandler;
	mCompletionRoutine.parameter = (void *) 0;  // not used

	mReadDataBuffer->setLength( mMaxPacketSize );

	// The way to set us up to recieve reads is to call it directly. Each time our read handler
	// is called, we'll have to do another to make sure we get the next read.

        retain();
	if( err = mInterruptPipe->Read( mReadDataBuffer, &mCompletionRoutine ) )
	{
		IOLog("****AppleProUSBbd::start - failed to do a read from the interrupt pipe.\n" );
                release();
		goto failedExit;
	}
	
	// Looks like we're OK, so return a success.
	
	return ( true );
	
failedExit:

	if( mPreparsedReportDescriptorData ) 
	{
        HIDCloseReportDescriptor( mPreparsedReportDescriptorData );
    }
	
	provider->close( this );
    stop( provider );
	
	// Return that we failed if we get here.

    return( false );
}

//====================================================================================================
// stop
//====================================================================================================

void AppleUSBProKbd::stop( IOService * provider )
{	
	if( mPreparsedReportDescriptorData ) 
	{
        HIDCloseReportDescriptor( mPreparsedReportDescriptorData );
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

void AppleUSBProKbd::ReadHandler( 	OSObject *	inTarget,
										void * 		inParameter,
										IOReturn	inStatus,
										UInt32		inBufferSizeRemaining )
{	
	AppleUSBProKbd * obj = (AppleUSBProKbd*) inTarget;
 
	switch ( inStatus )
    {
        case ( kIOReturnSuccess ):
		{
			// We got some good stuff, so jump right to our special
			// button handler.
            break;
		}
        case ( kIOReturnOverrun):
		{
            // Not sure what to do with this error.  It means more data
            // came back than the size of a descriptor.  Hmmm.  For now
            // just ignore it and assume the data that did come back is
            // useful.
			
            IOLog("****AppleUSBProKbd: overrun error.  ignoring.\n" );
            break;
		}
        case ( kIOReturnNotResponding ):
		{
            // This probably means the device was unplugged.  Now
            // we need to close the driver.
			
            // IOLog("****AppleUSBProKbd: Device unplugged.  Goodbye\n" );
            goto errorExit;
		}
 	case kIOReturnAborted:
	    // This generally means that we are done, because we were unplugged, but not always
            if (obj->fTerminating)
	    {
		// IOLog("%s: Read aborted. We are terminating\n", obj->getName());
		goto errorExit;
	    }
	    
	    IOLog("%s: Read aborted. Don't know why. Trying again\n", obj->getName());
	    goto queueAnother;
	    break;

       default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
			
            IOLog("****AppleUSBProKbd: error reading interrupt pipe\n" );
            goto queueAnother;
    }
	
	// Handle the data.
	
    obj->HandleSpecialButtons( (UInt8*) obj->mReadDataBuffer->getBytesNoCopy (),
								((UInt32) obj->mMaxPacketSize - inBufferSizeRemaining) );

queueAnother:

    // Reset the buffer.
	
    obj->mReadDataBuffer->setLength( obj->mMaxPacketSize );

    // Queue up another one before we leave.
	
    obj->retain();
    if( (inStatus = obj->mInterruptPipe->Read( obj->mReadDataBuffer, &obj->mCompletionRoutine )) )
    {
        // This is bad.  We probably shouldn't continue on from here.
		
        IOLog( "****AppleUSBProKbd: immediate error %d queueing read\n", inStatus );
        obj->release();
        goto errorExit;
    }

    obj->release();
    return;

errorExit:

    obj->mInterface->close( obj );
    obj->release();
    return;
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
		IOLog( "****AppleUSBProKbd::HandleSpecialButtons - HIDGetButtonsfailed. Error = %d.\n", (int) err );
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
					IOLog( "****AppleUSBProKbd::HandleSpecialButtons - no usage found for report. Usage = %d\n", 
							(int) usageList[reportIndex].usagePage );
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
	OSObject 	*adbProperty;
	const char 	*adbKey;
	
	
	mEventFlags = 0;
	mCapsLockOn = FALSE;
	
	// Get matching dictionary.
	
	matchingDictionary = IOService::serviceMatching( "IOHIKeyboard" );
	if( !matchingDictionary )
	{
		IOLog( "****AppleUSBProKeyboard - could not get a matching dictionary.\n" );
		goto exit;
	}
	
	// Get an iterator for the IOHIKeyboard devices.
	
	iterator = IOService::getMatchingServices( matchingDictionary );
	if( !iterator )
	{
		IOLog( "***AppleUSBProKeyboard - getMatchingServices failed.\n" );
		goto exit;
	}
	
	// User iterator to find devices and eject.
	//
	while( (device = (IOHIKeyboard*) iterator->getNextObject()) )
	{		
		//Ignore the eventFlags of non-keyboard ADB devices such as audio buttons
		adbProperty = device->getProperty("ADB Match");
		if (adbProperty)
		{
		    adbKey = ((OSString *)adbProperty)->getCStringNoCopy();
		    if( *adbKey != '2' )	//If not a keyboard
			continue;
		}

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
			// IOLog("%s::message - service is terminated - aborting pipe\n", getName());
			fTerminating = TRUE;		// this will cause us to close
			if (mInterruptPipe)
			mInterruptPipe->Abort();
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

