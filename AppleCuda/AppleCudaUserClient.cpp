/*
* Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOMessage.h>
#include "AppleCudaUserClient.h"

#ifndef NULL
#define NULL	0
#endif

#define super	IOUserClient

OSDefineMetaClassAndStructors(AppleCudaUserClient, IOUserClient)

AppleCudaUserClient*
AppleCudaUserClient::withTask(task_t owningTask)
{
AppleCudaUserClient *client;

    client = new AppleCudaUserClient;
    if (client != NULL)
	{
        if (client->init() == false)
		{
            client->release();
            client = NULL;
        }
    }
    if (client != NULL)
	{
        client->fTask = owningTask;
    }

    return ( client );
}





bool
AppleCudaUserClient::start(IOService *provider)
{
bool result = false;

    theInterface = OSDynamicCast(AppleCuda, provider);
    if (theInterface != NULL)
	{
        result = super::start( provider );
		if ( result == false )
		{
			IOLog( "%s::start - provider start failed\n", getName() );
		}
	}

    return( result );
}






IOReturn
AppleCudaUserClient::clientClose(void)
{
    if ( theInterface )			// object pointer to our provider
    {
        detach( theInterface );
        theInterface = NULL;
    }
    return( kIOReturnSuccess );
}






IOReturn
AppleCudaUserClient::clientDied(void)
{
    return( clientClose() );
}






IOReturn
AppleCudaUserClient::connectClient(IOUserClient *client)
{
    return( kIOReturnSuccess );
}






IOReturn
AppleCudaUserClient::registerNotificationPort(mach_port_t port, UInt32 type)
{
    return( kIOReturnUnsupported );
}






// --------------------------------------------------------------------------
// Method: setProperties
//
// Purpose:
//       sets the property from the dictionary to the device's properties.

IOReturn
AppleCudaUserClient::setProperties( OSObject * properties )
{
OSDictionary *	dict;

    dict = OSDynamicCast( OSDictionary, properties );
    if ((dict) && (theInterface != NULL))
	{
	OSData *data;

        // Sets the wake on ring:
        if( (data = OSDynamicCast( OSData, dict->getObject( "WakeOnRing" )) ) )
		{
		UInt8 myBool = *((UInt8*)data->getBytesNoCopy());

			//Disabled because never tested with OS X sleep/wake mechanism
            // (not to mention Cuda does not support it)
            // theInterface->setWakeOnRing( myBool );
            IOLog("%s::setProperties WakeOnRing %d (NOT SUPPORTED)\n",getName(), myBool);
            
            // returns Option not supported:
            return( kIOReturnUnsupported );
        }

        // Sets the "auto-restart on power loss?" mode:
        if( (data = OSDynamicCast( OSData, dict->getObject("FileServer")) ) )
		{
		UInt8 myBool = *((UInt8*)data->getBytesNoCopy());

            theInterface->setFileServerMode( myBool );
            
            // returns success:
            return( kIOReturnSuccess );
        }

		//Demand sleep immediately:
        if( (data = OSDynamicCast( OSData, dict->getObject( "SleepNow" )) ) )
		{
		UInt8 myBool = *((UInt8*)data->getBytesNoCopy());
	    
			if ( myBool )
			{
				theInterface->demandSleepNow();
				IOLog( "%s::setProperties SleepNow\n", getName() );
			}
			return( kIOReturnSuccess );
		}

        // Sets the self-wake time:
        // FOR LEGACY SUPPORT - AutoWake
        // In OS X 10.1 Puma and 10.2 Jaguar, several third party software developers started using
        // the ApplePMU user client to directly set AutoWake and AutoPower times.
        // OS X Panther offers a higher level queueing service to schedule multiple power on eventns
        // in the future. We re-direct all direct setProperty PMU AutoWake/AutoPower calls
        // to the PM user code, which will enqueue this request as if it were made using the existing API.
        if( (data = OSDynamicCast( OSData, dict->getObject("AutoWake")) ) )
	{
		UInt32 newTime;
		IOByteCount len = data->getLength();

            if ( len == sizeof( UInt32 )  )
                newTime = *((UInt32*)data->getBytesNoCopy());
            else
                newTime = 0;
                
             // Redirects AutoWake request to interested clients. PowerManagement configd plugin is listening.
            theInterface->messageClients(kIOPMUMessageLegacyAutoWake, (void *)newTime);

	    //theInterface->setWakeTime( newTime * 1000 ); //convert to milliseconds
            // IOLog( "%s::setProperties IOPMUMessageLegacyAutoWake 0x%08lx\n", getName(), newTime );   

            // returns success:
            return( kIOReturnSuccess );
        }
     
 
        // Sets the self-wake time for OS client (PowerManagement or SleepCycle type app calls here):
        if( (data = OSDynamicCast( OSData, dict->getObject("AutoWakePriv"))))
	{
            UInt32 newTime;
            IOByteCount len = data->getLength();

            if ( len == sizeof( UInt32 )  )
                newTime = *((UInt32*)data->getBytesNoCopy());
            else
                newTime = 0;
                
            theInterface->setWakeTime( newTime * 1000 ); //convert to milliseconds
            //IOLog( "%s::setProperties AutoWakePriv 0x%08lx\n", getName(), newTime );   

            // returns success:
            return( kIOReturnSuccess );
        }

        // FOR LEGACY SUPPORT - AutoPower
        if( (data = OSDynamicCast( OSData, dict->getObject("AutoPower")) ) )
	{
		UInt32 newTime;
		IOByteCount len = data->getLength();

            if (len == sizeof( UInt32 ) )
                newTime = *((UInt32*)data->getBytesNoCopy());
            else
                newTime = 0;
                
            // Redirects AutoPower request to interested clients. PowerManagement configd plugin is listening.
            theInterface->messageClients(kIOPMUMessageLegacyAutoPower, (void *)newTime);

            //theInterface->setPowerOnTime( newTime );
            //IOLog( "%s::setProperties IOPMUMessageLegacyAutoPower 0x%08lx\n", getName(), newTime );   

            // returns success:
            return( kIOReturnSuccess );
        }
        
        
        // Sets the self-poweron time (PowerManagement or Restarter type app calls here):
        if( (data = OSDynamicCast( OSData, dict->getObject("AutoPowerPriv"))))
	{
            UInt32 newTime;
            IOByteCount len = data->getLength();

            if (len == sizeof( UInt32 ) )
                newTime = *((UInt32*)data->getBytesNoCopy());
            else
                newTime = 0;

            theInterface->setPowerOnTime( newTime );
            //IOLog( "%s::setProperties AutoPowerPriv 0x%08lx\n", getName(), newTime );   

            // returns success:
            return( kIOReturnSuccess );
        }



    }

    return( kIOReturnBadArgument );
}
