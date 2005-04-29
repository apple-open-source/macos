#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "disk_power.h"

int PowerStateSummary( IOATAPowerState powerState );

/*
-- PowerStateString()
*/

char * PowerStateString( IOATAPowerState x, int opt_summary )
{
	char * result;

	if ( opt_summary )
	{
		switch ( PowerStateSummary( x ) )
		{
			case -1:
				result = "ON";
			break;
			case 0: // This is the only value where the two scales collide.
				result = "OFF";
			break;
			default:
				fprintf(stderr, "ERROR: %s: unknown IOATAPowerState %d\n", __FUNCTION__, (int)x);
				result = "UNKNOWN";
			break;
		}
	}
	else
	{
		switch ( x )
		{
			case kIOATAPowerStateSystemSleep: // This is the only value where the two scales collide.
				result = "SystemSleep";
			break;
			case kIOATAPowerStateSleep:
				result = "Sleep";
			break;
			case kIOATAPowerStateStandby:
				result = "Standby";
			break;
			case kIOATAPowerStateIdle:
				result = "Idle";
			break;
			case kIOATAPowerStateActive:
				result = "Active";
			break;
			default:
				fprintf(stderr, "ERROR: %s: unknown IOATAPowerState %d\n", __FUNCTION__, (int)x);
				result = "UNKNOWN";
			break;
		}
	}

	return result;

} // PowerStateString()

/*
-- PowerStatesMax()
*/

IOATAPowerState PowerStatesMax( IOATAPowerStates * powerStates )
{
	IOATAPowerState driverDesire = powerStates->driverDesire;
	IOATAPowerState deviceDesire = powerStates->deviceDesire;
	IOATAPowerState userDesire   = powerStates->userDesire;

	IOATAPowerState maxState = 0; 
	
	if ( driverDesire > maxState ) maxState = driverDesire;
	
	if ( deviceDesire > maxState ) maxState = deviceDesire;
	
	if ( userDesire   > maxState ) maxState = userDesire;

	return maxState;

} // PowerStatesMax()
                
/*
-- PowerStateSummary()
*/

/* Returns
-- -1 == ON
--  0 == OFF
-- Can be used together with the positive values denoting IOATAPowerState's.
-- But you have to be careful not to confuse with OFF == 0 == kIOATAPowerStateSystemSleep.
*/

int PowerStateSummary( IOATAPowerState powerState )
{
	int result;
	
	// Summarizing twice does nothing.  Idempotent.
	if ( powerState <= 0 )
		result = powerState;
	else
#if 1
	if ( 0 <= powerState && powerState <= kIOATAPowerStateSleep )
		result = 0;
	else
		result = -1;
#else
	if ( 0 <= powerState && powerState <= kIOATAPowerStateStandby ) // Spun down.
		result = 0; // OFF
	else
	if ( kIOATAPowerStateIdle <= powerState && powerState <= kIOATAPowerStateActive ) // Spun up.
		result = -1; // ON
	else
	{
		fprintf(stderr, "ERROR: %s(%d): unexpected value.\n", __FUNCTION__, powerState);
		exit(-1);
	}
#endif

	return result;

} // PowerStateSummary()

/*
-- GetATADeviceInfo()
*/

// Fairly often this returns -11, meaning that it was unable to locate a matching device.
// If this happens, just wait awhile and try again.
//
// See GetATADeviceInfoWithRetry()
//

int GetATADeviceInfo( DiskDevice * device )
{
	int result;

	IOATAPowerStates * powerStates = & device->powerStates;
	
    kern_return_t kr;
    mach_port_t masterPort;
    io_registry_entry_t service;
    io_iterator_t iterator;
    
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    assert(KERN_SUCCESS==kr);
        
    /* look for drives */
    IOServiceGetMatchingServices(masterPort, IOServiceMatching ( "ATADeviceNub" ), & iterator );
    if ( ! iterator )
    {
    	result = -10;
    	goto Return;
    }

    while (( service = IOIteratorNext( iterator ) ))
	{
		CFStringRef str = nil;
		CFMutableDictionaryRef properties = nil;
		CFDictionaryRef physCharacteristics = nil;
		io_iterator_t child_iterator;
		io_registry_entry_t child;

		// Device Model
		
		char deviceModel[ 256 ];
		bzero( deviceModel, sizeof deviceModel );

		str = IORegistryEntryCreateCFProperty( service, CFSTR("device model"), kCFAllocatorDefault, kNilOptions );
		if ( str )
		{
			CFStringGetCString( str, deviceModel, sizeof deviceModel, kCFStringEncodingMacRoman );
			CFRelease( str );
		}

		// Device Interconnect & Device Location
		
		char deviceInterconnect[ 256 ];
		bzero(deviceInterconnect, sizeof deviceInterconnect );
		char deviceLocation[ 256 ];
		bzero(deviceLocation, sizeof deviceLocation );

		IORegistryEntryCreateCFProperties( service, & properties, kCFAllocatorDefault, kNilOptions );
		if ( properties )
		{
			physCharacteristics = CFDictionaryGetValue( properties, CFSTR("Protocol Characteristics") );
			if ( physCharacteristics )
			{
				// device interconnect 
				str = CFDictionaryGetValue( physCharacteristics, CFSTR("Physical Interconnect") );
				if ( str )
				{
					CFStringGetCString( str, deviceInterconnect, sizeof deviceInterconnect, kCFStringEncodingMacRoman );
				}
		
				// device location
				str = CFDictionaryGetValue( physCharacteristics, CFSTR("Physical Interconnect Location") );
				if ( str )
				{
					CFStringGetCString( str, deviceLocation, sizeof deviceLocation, kCFStringEncodingMacRoman );
				}
			}
			
			CFRelease( properties );
		}
		
		IORegistryEntryGetChildIterator( service, kIOServicePlane, & child_iterator );
		while (( child = IOIteratorNext( child_iterator ) ))
		{
			int driverDesire, deviceDesire, userDesire;
			
			// fill in interconnect info if we don't already have it
			if ( 0 == strlen(deviceInterconnect) )
			{
				str = IORegistryEntryCreateCFProperty( child, CFSTR("Physical Interconnect"), kCFAllocatorDefault, kNilOptions );
				if ( str )
				{
					CFStringGetCString( str, deviceInterconnect, sizeof deviceInterconnect, kCFStringEncodingMacRoman );
					CFRelease( str );
				}
			}
	
			if ( 0 == strlen( deviceLocation ) )
			{
				str = IORegistryEntryCreateCFProperty( child, CFSTR("Physical Interconnect Location"), kCFAllocatorDefault, kNilOptions );
				if ( str )
				{
					CFStringGetCString( str, deviceLocation, sizeof deviceLocation , kCFStringEncodingMacRoman );
					CFRelease( str );
				}
			}
			
			// Device Type
			
			char deviceType[ 256 ];
			bzero( deviceType, sizeof deviceType );

			// Power State
			
			char powerState[ 256 ];
			bzero( powerState, sizeof powerState );
			
			// find out what type of device this is - ATAPI will be added as SCSI devices
			str = IORegistryEntryCreateCFProperty( service, CFSTR("ata device type"), kCFAllocatorDefault, kNilOptions );
			if ( str )
			{
				CFStringGetCString( str, deviceType, sizeof deviceType, kCFStringEncodingMacRoman );
				CFRelease( str );
				
				if ( 0 == strcmp( deviceType, "ata" ) ) // regular ATA disks (not ATAPI)
				{
					IORegistryEntryCreateCFProperties( child, & properties, kCFAllocatorDefault, kNilOptions );
					if ( properties )
					{
						str = CFDictionaryGetValue( properties, CFSTR("Power Management private data") );
						if ( str )
						{
							CFStringGetCString( str, powerState, sizeof powerState, kCFStringEncodingMacRoman );
						}
						CFRelease( properties );
					}
				}
			}
			
			if ( 3 == sscanf	(	powerState,
									"{ this object = %*x, interested driver = %*x, driverDesire = %d, deviceDesire = %d, ourDesiredPowerState = %d, previousRequest = %*d }",
									& driverDesire, & deviceDesire, & userDesire
								)
			)
			{
				device->timestamp = time( NULL );

				device->name         = strdup( deviceModel );        // copy of the original
				device->location     = strdup( deviceLocation );     // copy of the original
				device->interconnect = strdup( deviceInterconnect ); // copy of the original

				powerStates->driverDesire = driverDesire;
				powerStates->deviceDesire = deviceDesire;
				powerStates->userDesire   = userDesire;

				IOObjectRelease( child_iterator );
			    IOObjectRelease( iterator );
			    
			    result = 0;
				goto Return;
			}

		} // while (child...)
		
		IOObjectRelease( child_iterator );

	} // while (service...)

    IOObjectRelease( iterator );
    
    result = -11;
    goto Return;
    
Return:
	return result;
	
} // GetATADeviceInfo()


/*
-- GetATADeviceInfoWithRetry()
*/

// Devices are often momentarily busy, e.g., when spinning up.  Retry up to 10 times, 1 second apart.

int GetATADeviceInfoWithRetry( DiskDevice * diskDevice )
{
	int err;
	
	int retryNumber;
	
	for ( retryNumber = 0; retryNumber < 10; retryNumber++ )
	{
		err = GetATADeviceInfo( diskDevice );
		if ( noErr == err )
		{
			goto Return;
		}
#if 0
		char errorStringBuffer[ 256 ];
		char * errorString = errorStringBuffer;
		errorString += sprintf_timestamp_now( errorString );
		errorString += sprintf(errorString, ": WARNING: %s: sleeping and retrying...\n", __FUNCTION__);
		fputs( errorStringBuffer, stderr );
		fflush(stdout);
#endif
		sleep(1);
	}

	// Failure.
	goto Return;
	
Return:
	return err;

} // GetATADeviceInfoWithRetry()

