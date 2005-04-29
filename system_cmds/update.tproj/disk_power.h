#ifndef __SPINDOWN_H__

#define __SPINDOWN_H__

#include <libkern/OSTypes.h>
#include <IOKit/storage/ata/IOATAStorageDefines.h>
#include <stdio.h>
#include <time.h>

/* IOATAPowerStates */

struct IOATAPowerStates
{
    IOATAPowerState driverDesire;
    IOATAPowerState deviceDesire;
    IOATAPowerState userDesire;
};

typedef struct IOATAPowerStates IOATAPowerStates;

/* DiskDevice: see GetATADeviceInfo(), GetATADeviceInfoWithRetry() */

struct DiskDevice
{
	time_t timestamp;
    char * name;
    char * location;
    char * interconnect;
    IOATAPowerStates powerStates;
};

typedef struct DiskDevice DiskDevice;

/* Devices */


/* PowerStatesMax()
-- Returns the max-of-three power state "desires" from the given IOATAPowerStates.
*/
IOATAPowerState PowerStatesMax( IOATAPowerStates * powerStates );

/* PowerStateString()
-- Returns human-readable string name for the given IOATAPowerState.
-- Returns pointer to a static, const string.
*/
char * PowerStateString( IOATAPowerState x, int opt_summary );

/* Routines for printing fs_usage-compatible timestamps: HH:MM:SS.000
*/
int    sprintf_timestamp( char * str, time_t t );
int    sprintf_timestamp_now( char * str );
char * timestampStr_static( time_t t ); /* Returns pointer to a static, const string. */

#if 0
/* Routines for printing time intervals: HH:MM:SS
*/
int    sprintf_interval( char * str, time_t interval );
char * intervalStr_static( time_t interval ); /* Returns pointer to a static, const string. */
#endif

/* GetATADeviceInfoWithRetry()
-- This invokes GetATADeviceInfo() repeatedly at one second intervals until it returns without error.
-- Gives up after 10 tries, so it is possible that this could return an error.
*/
int    GetATADeviceInfoWithRetry( DiskDevice * diskDevice );

/*
--
*/

#endif /* __SPINDOWN_H__ */
