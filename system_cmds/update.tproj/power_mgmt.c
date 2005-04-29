
#include "disk_power.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>

#include <CoreFoundation/CFNumber.h>

/*
--
*/

int on_battery_power( void )
{
	int result = 0; // Assume we're not running on battery power.
    kern_return_t kr;
    mach_port_t master_device_port;

	CFArrayRef cfarray = NULL;

    kr = IOMasterPort( bootstrap_port, & master_device_port);
    if ( KERN_SUCCESS != kr)
    {
		fprintf(stderr,"ERROR: IOMasterPort() failed\n");
    	goto Return;
    }

	kr = IOPMCopyBatteryInfo( master_device_port, & cfarray );
    if ( kIOReturnSuccess != kr  )
    {
    	// This case handles desktop machines in addition to error cases.
    	result = 0;
    	goto Return;
    }

	{
		CFDictionaryRef dict;
		CFNumberRef cfnum;
		int flags;

		dict = CFArrayGetValueAtIndex( cfarray, 0 );
		cfnum = CFDictionaryGetValue( dict, CFSTR(kIOBatteryFlagsKey) );
		
		if ( CFNumberGetTypeID() != CFGetTypeID( cfnum ) )
		{
			fprintf(stderr, "ERROR: on_battery_power(): battery flags not a CFNumber!!!\n");
			result = 0;
			goto Return;
		}

		CFNumberGetValue( cfnum, kCFNumberLongType, & flags );

#if 0
		printf( "BatteryFlags          = %#08x\n", flags );

		printf( "BatteryChargerConnect : %s\n", ( 0 != ( flags & kIOBatteryChargerConnect ) ) ? "TRUE" : "FALSE" );
		printf( "BatteryCharge         : %s\n", ( 0 != ( flags & kIOBatteryCharge ) )         ? "TRUE" : "FALSE" );
		printf( "BatteryInstalled      : %s\n", ( 0 != ( flags & kIOBatteryInstalled ) )      ? "TRUE" : "FALSE" );
#endif

		result = ( 0 == ( flags & kIOPMACInstalled ) );
	}

Return:
	if (cfarray)
			CFRelease(cfarray);
	return result;

} // on_battery_power()

/*
--
*/

int is_disk_awake( void )
{
	int result = 1; // Go ahead and sync by default.
	int err;
	DiskDevice diskDevice;
	IOATAPowerState powerState;

	bzero(&diskDevice, sizeof(diskDevice));

	err = GetATADeviceInfoWithRetry( & diskDevice );
	if ( err )
	{
		result = 1; // Go ahead and sync.
		goto Return;
	}

	powerState = PowerStatesMax( & diskDevice.powerStates );

//	printf( "%s: power state = %s\n", __FUNCTION__, PowerStateString( powerState, /* opt_summary = */ 0 ) );

	result = ( powerState >= kIOATAPowerStateStandby );

Return:
	if (diskDevice.name)
		free(diskDevice.name);
	if (diskDevice.location)
		free(diskDevice.location);
	if (diskDevice.interconnect)
		free(diskDevice.interconnect);

//	printf( "%s => %s\n", __FUNCTION__, result ? "TRUE" : "FALSE" );
	return result;

} // is_disk_awake()

/*
--
*/

