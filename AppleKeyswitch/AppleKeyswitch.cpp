/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2006 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman / Bill Galcher
 *
 */

#include "AppleKeyswitch.h"

#define super IOService
OSDefineMetaClassAndStructors(AppleKeyswitch, IOService);



/* **************************************************
*	    s t a r t                                   *
* **************************************************/

bool AppleKeyswitch::start( IOService * provider )
{
    UInt8 		val = 0;
#if defined( __ppc__ )										// **********************  PowerPC -- Start
    OSData 		*tempOSData;
	IOReturn	status;
#endif														// **********************  PowerPC -- End

	DLOG("AppleKeyswitch::start entered.\n");

    if ( ! super::start(provider) )
	{
		DLOG( "AppleKeyswitch::start - super::start() failed\n" );
        return false;
	}

#if defined( __i386__ ) || defined( __x86_64__ )			// **********************  Intel -- Start

	//myProvider = OSDynamicCast( IOACPIPlatformDevice, provider );
	myProvider = (IOACPIPlatformDevice *) provider->metaCast( "IOACPIPlatformDevice" );
	if ( ! myProvider )
	{
		DLOG( "AppleKeyswitch::start - metaCast( 'IOACPIPlatformDevice' ) failed\n" );
		return false;
	}

	if ( ! myProvider->open(this) )
	{
		IOLog( "AppleKeyswitch::start - open() failed\n" );
		return false;
	}

#endif														// **********************  Intel -- End

    myWorkLoop = (IOWorkLoop *)getWorkLoop();
	if(myWorkLoop == NULL)
    {     
        IOLog("AppleKeyswitch::start failed to get WorkLoop.\n" );
		return false;
    }

#if defined( __ppc__ )										// **********************  PowerPC -- Start

	DLOG( "AppleKeyswitch::start - PPC-specific initialization entered.\n" );

    interruptSource = IOInterruptEventSource::interruptEventSource(this, (IOInterruptEventAction)&AppleKeyswitch::interruptOccurred, provider, 0);
    if (interruptSource == NULL)
    {     
        IOLog("AppleKeyswitch::start failed to create interrupt event source.\n");
		return false;
    }

    status = myWorkLoop->addEventSource(interruptSource);
    if (status != kIOReturnSuccess)
    {
        IOLog("AppleKeyswitch::start failed to add interrupt event source to work loop.\n");
		return false;
    }

    // callPlatformFunction symbols
    keyLargo_safeWriteRegUInt8 = OSSymbol::withCString("keyLargo_safeWriteRegUInt8");
    keyLargo_safeReadRegUInt8  = OSSymbol::withCString("keyLargo_safeReadRegUInt8");

    // Get ExtIntGPIO where interrupt is coming in.
    tempOSData = OSDynamicCast(OSData, provider->getProperty("reg"));
	if (tempOSData == NULL)
    {
        IOLog("AppleKeyswitch::start - failed to get GPIO offset value.\n");     
		return false;
    }
    
    extIntGPIO = (UInt32*)tempOSData->getBytesNoCopy();

    // Configure the ExtIntGPIO for interrupts
    callPlatformFunction(keyLargo_safeWriteRegUInt8, false, (void *)(kKeyLargoGPIOBaseAddr + *extIntGPIO), (void *)0xFF, (void *)0x80, (void *)0);

    IOSleep(100);
    
    // Read initial keyswitch state
	getKeyswitchState( &val );	// returns 0 (locked) or 1 (unlocked) in -val-

    // Enable interrupt
    interruptSource->enable();

															// **********************  PowerPC -- End
#elif defined( __i386__ ) || defined( __x86_64__ )			// **********************  Intel   -- Start

	DLOG( "AppleKeyswitch::start - Intel-specific initialization entered.\n" );

													//       type                handler        target ref
	switchEventNotify = myProvider->registerInterest( gIOGeneralInterest, keyswitchNotification, this,  0 );

	if ( ! switchEventNotify )
	{
		IOLog( "AppleKeyswitch::start - Unable to register interest in getting keyswitch notifications\n" );
		stop( provider );
		return false;
	}

	getKeyswitchState( &val );	// returns 0 (locked) or 1 (unlocked) in -val-

#endif														// **********************  Intel -- End

	DLOG( "AppleKeyswitch::start - Initial keyswitch value: 0x%02X (%s)\n", val, ( val == KEYSWITCH_VALUE_UNLOCKED )?  "UNLOCKED" : "LOCKED" );

	if( val == KEYSWITCH_VALUE_UNLOCKED )
    {
		DLOG( "AppleKeyswitch::start - Initial keyswitch state is UNLOCKED\n" );
        state = kSWITCH_STATE_IS_UNLOCKED;
        setProperty("Keyswitch", false);
    }
    else
    {
		DLOG( "AppleKeyswitch::start - Initial keyswitch state is LOCKED\n" );
        state = kSWITCH_STATE_IS_LOCKED;
        setProperty("Keyswitch", true);
    }


    registerService();

	DLOG("AppleKeyswitch::start - exit.\n");

    return true;
}




/* **************************************************
*	    s t o p                                     *
* **************************************************/

void AppleKeyswitch::stop(IOService *provider)
{
#if defined( __ppc__ )										// **********************  PowerPC -- Start

    interruptSource->disable();
    myWorkLoop->removeEventSource(interruptSource);

    if (interruptSource != NULL)
    {
        interruptSource->release();
        interruptSource = NULL;
    }
															// **********************  PowerPC -- End
#elif defined ( __i386__ ) || defined( __x86_64__ )			// **********************  Intel -- Start

	if ( switchEventNotify )
	{
		DLOG( "AppleKeyswitch::stop - de-registering interest in notifications\n" );
		switchEventNotify->remove();
		switchEventNotify = 0;
	}

#endif														// **********************  Intel -- End

    if (myWorkLoop != NULL)
    {
        myWorkLoop->release();
        myWorkLoop = NULL;
    }
    
    super::stop(provider);
}




#if defined( __ppc__ )										// **********************  PowerPC -- Start

/* **************************************************
*	    i n t e r r u p t O c c u r r e d           *
* **************************************************/

void AppleKeyswitch::interruptOccurred(OSObject* obj, IOInterruptEventSource * source, int count)
{
    AppleKeyswitch *AppleKeyswitchPtr = (AppleKeyswitch *)obj;

    if (AppleKeyswitchPtr != NULL)
        AppleKeyswitchPtr->toggle(false);
    
    return;
}

#endif														// **********************  PowerPC -- End


#if	defined( __i386__ ) || defined( __x86_64__ )			// **********************  Intel -- Start

/* **************************************************
*	    k e y s w i t c h N o t i f i c a t i o n   *
* **************************************************/

IOReturn
AppleKeyswitch::keyswitchNotification(	void *      target,
										void *      refCon,
										UInt32      messageType,
										IOService * provider,
										void *      messageArgument,
										vm_size_t   argSize )
{
AppleKeyswitch	* me  = (AppleKeyswitch *) target;

	if ( me )
	{
		if ( (messageType == kIOACPIMessageDeviceNotification) && messageArgument )
		{
			UInt32  notifyCode = *(UInt32 *) messageArgument;

			DLOG( "AppleKeyswitch::keyswitchNotification - notification received.  notifyCode = %lX\n", notifyCode );

			if ( notifyCode == 0x80 )	// ick -- magic number.  we hate magic numbers
				me->toggle( false );
		}
	}

    return kIOReturnSuccess;

}
#endif														// **********************  Intel -- End



/* **************************************************
*	   t o g g l e                                  *
* **************************************************/

void AppleKeyswitch::toggle( bool disableInts )
{
UInt8 	val;


#if defined( __ppc__ )										// **********************  PowerPC -- Start
UInt8	total = 0;
int		i;

    // Disable KeyLargo ExtInt_GPIO4 interrupts
    if (disableInts)
        interruptSource->disable(); 

    // Get 100 samples and determine state of keyswitch (debounce)
    for(i=0; i<=100; i++)
    {
        // get 100 samples
        //callPlatformFunction(keyLargo_safeReadRegUInt8, false, (void *)(kKeyLargoGPIOBaseAddr + *extIntGPIO), (void *)&val, (void *)0, (void *)0);
		getKeyswitchState( &val );	// returns 0 (locked) or 1 (unlocked) in -val-
        total += val;
        IOSleep(1);
    }

	// define PREVIOUS switch state?
    if( total > 80 )
	{
        state = kSWITCH_STATE_IS_UNLOCKED;
		DLOG( "AppleKeyswitch::toggle - changing state to UNLOCKED (%d)\n", state );
        // set Keyswitch property and registerService
        setProperty("Keyswitch", false);
		registerService();
	}
    else if( total < 20 )
    {
        state = kSWITCH_STATE_IS_LOCKED;
		DLOG( "AppleKeyswitch::toggle - changing state to LOCKED (%d)\n", state );
        
        // set Keyswitch property and registerService
        setProperty("Keyswitch", true);
		registerService();
    }
	else
	{
		DLOG( "AppleKeyswitch::toggle - lock state does not yet debounced; leaving state alone\n" );
	}

    // Enable KeyLargo ExtInt_GPIO4 interrupts
    if (disableInts)
        interruptSource->enable(); 
	
															// **********************  PowerPC -- End
#elif defined ( __i386__ ) || defined( __x86_64__ )			// **********************  Intel -- Start

	// ACPI is going to do the interrupt / switch-debounce for us.
	// All we have to do is read the switch value.

	val = 0;
	getKeyswitchState( &val );	// returns 0 (locked) or 1 (unlocked)

	// since it is debounced for us by ACPI, it will either be 0 (locked) or 1 (unlocked)

	if( val == KEYSWITCH_VALUE_UNLOCKED )
	{
		DLOG( "AppleKeyswitch::toggle - changing state to UNLOCKED (%d)\n", state );
		state = kSWITCH_STATE_IS_UNLOCKED;
		setProperty("Keyswitch", false);
	}
	else	// the beauty of boolean values - if its not unlocked, then its locked
	{
		DLOG( "AppleKeyswitch::toggle - changing state to LOCKED (%d)\n", state );
		state = kSWITCH_STATE_IS_LOCKED;
		setProperty("Keyswitch", true);
	}

	registerService();

#endif														// **********************  Intel -- End
   
    return;
}





/* **************************************************
*	    g e t K e y s w i t c h S t a t e           *
* ***************************************************
*
*	Retrieves and returns the current value of the keyswitch.
*
*	Returns:	0	locked
*				1	unlocked
*/

void AppleKeyswitch::getKeyswitchState( UInt8 * switchState )
{
UInt8		val = 0;	// default value is "unlocked".  should it be "locked"?

#if defined( __ppc__ )										// **********************  PowerPC -- Start

    callPlatformFunction(keyLargo_safeReadRegUInt8, false, (void *)(kKeyLargoGPIOBaseAddr + *extIntGPIO), (void *)&val, (void *)0, (void *)0);
	DLOG( "AppleKeyswitch::getKeyswitchState(ppc) - value read from property: 0x%02X\n", val );
	val = ( val & 0x2 ) >> 1;

															// **********************  PowerPC -- End
#elif defined ( __i386__ ) || defined( __x86_64__ )			// **********************  Intel -- Start

IOReturn	err;
UInt32		num = 0;

	err = myProvider->evaluateInteger( "KLCK", &num );
	if ( err == kIOReturnSuccess )
	{
		DLOG( "AppleKeyswitch::getKeyswitchState(i386) - value read from property: 0x%08lX\n", num );
		// ACPI returns "is this locked?" (1 = true, 0 = false)
		// apparently the MacIO GPIO was active low, meaning 0 = LOCKED, 1 = UNLOCKED
		// So - invert the sense of the returned bit to make the subsequent logic work correctly.
		val = (UInt8)  1 - num;
	}
	else
	{
		DLOG( "AppleKeyswitch::getKeyswitchState(i386) - evaluateInteger did NOT succeed (%d)\n", err );
	}


#endif														// **********************  Intel -- End

	* switchState = val;

	DLOG( "AppleKeyswitch::getKeyswitchState - value returned to caller: 0x%02X\n", val );

    return;
}