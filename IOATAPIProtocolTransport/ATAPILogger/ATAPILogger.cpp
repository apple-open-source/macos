/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').	You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libutil.h>

#include <mach/clock_types.h>
#include <mach/mach_time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/

#include <IOKit/scsi/IOSCSIArchitectureModelFamilyTimestamps.h>
#include <IOKit/scsi/IOSCSIArchitectureModelFamilyDebugging.h>
#include <IOKit/storage/ata/IOATAPIProtocolTransportTimeStamps.h>
#include <IOKit/storage/ata/IOATAPIProtocolTransportDebugging.h>

#define DEBUG 			0


//-----------------------------------------------------------------------------
//	Structures
//-----------------------------------------------------------------------------

typedef struct SCSITaskLogEntry
{
	TAILQ_ENTRY(SCSITaskLogEntry)	chain;
	unsigned int					taskID;
	uint8_t							cdb[16];
} SCSITaskLogEntry;


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

enum
{
	kSCSITaskTracePointCDBLog1Code	 		= SCSITASK_TRACE ( kSCSITaskTracePointCDBLog1 ),
	kSCSITaskTracePointCDBLog2Code 			= SCSITASK_TRACE ( kSCSITaskTracePointCDBLog2 ),
	kSCSITaskTracePointResponseLog1Code		= SCSITASK_TRACE ( kSCSITaskTracePointResponseLog1 ),
	kSCSITaskTracePointResponseLog2Code		= SCSITASK_TRACE ( kSCSITaskTracePointResponseLog2 ),
};


enum
{
	kATADeviceInfoCode						= ATAPI_TRACE( kATADeviceInfo ), 
	kATASendSCSICommandCode					= ATAPI_TRACE( kATASendSCSICommand ), 
	kATASendSCSICommandFailedCode			= ATAPI_TRACE( kATASendSCSICommandFailed ), 
	kATACompleteSCSICommandCode				= ATAPI_TRACE( kATACompleteSCSICommand ), 
	kATAAbortCode							= ATAPI_TRACE( kATAAbort ), 
	kATAResetCode							= ATAPI_TRACE( kATAReset ), 
	kATAResetCompleteCode					= ATAPI_TRACE( kATAResetComplete ), 
	kATAHandlePowerOnCode					= ATAPI_TRACE( kATAHandlePowerOn ), 
	kATAPowerOnResetCode					= ATAPI_TRACE( kATAPowerOnReset ), 
	kATAPowerOnNoResetCode					= ATAPI_TRACE( kATAPowerOnNoReset ), 
	kATAHandlePowerOffCode					= ATAPI_TRACE( kATAHandlePowerOff ), 
	kATADriverPowerOffCode					= ATAPI_TRACE( kATADriverPowerOff ), 
};


#define kTraceBufferSampleSize			60000
#define kMicrosecondsPerSecond			1000000
#define kMicrosecondsPerMillisecond		1000


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

int					gBiasSeconds		= 0;
double				gDivisor 			= 0.0;		/* Trace divisor converts to microseconds */
kd_buf *			gTraceBuffer		= NULL;
boolean_t			gTraceEnabled 		= FALSE;
boolean_t			gSetRemoveFlag		= TRUE;
boolean_t			gVerbose			= FALSE;
boolean_t			gEnableTraceOnly 	= FALSE;
const char *		gProgramName		= NULL;
uint32_t			gPrintfMask			= 0;
uint32_t			gSavedSAMTraceMask	= 0;
uint32_t			gSavedATAPIDebugMask	= 0;

TAILQ_HEAD(SCSITaskLogEntryHead, SCSITaskLogEntry) gListHead = TAILQ_HEAD_INITIALIZER(gListHead);

//-----------------------------------------------------------------------------
//	Prototypes
//-----------------------------------------------------------------------------

static void
EnableTraceBuffer ( int val );

static void
CollectTrace ( void );

static void
SignalHandler ( int signal );

static void
GetDivisor ( void );

static void
RegisterSignalHandlers ( void );

static void
AllocateTraceBuffer ( void );

static void
RemoveTraceBuffer ( void );

static void
SetTraceBufferSize ( int nbufs );

static void
Quit ( const char * s );

static void
InitializeTraceBuffer ( void );

static void
ParseArguments ( int argc, const char * argv[] );

static void
PrintUsage ( void );

static void
LoadATAPIExtension ( void );

static void
ResetDebugFlags ( void );


//-----------------------------------------------------------------------------
//	Main
//-----------------------------------------------------------------------------

int
main ( int argc, const char * argv[] )
{
	
	SAMSysctlArgs 	samArgs;
	ATAPISysctlArgs atapiArgs;
	int				error;
	
	gProgramName = argv[0];
	
	if ( geteuid ( ) != 0 )
	{
		
		fprintf ( stderr, "'%s' must be run as root...\n", gProgramName );
		exit ( 1 );
		
	}
	
	if ( reexec_to_match_kernel ( ) )
	{
		
		fprintf ( stderr, "Could not re-execute to match kernel architecture. (Error = %d)\n", errno );
		exit ( 1 );
		
	}
	
	// Get program arguments.
	ParseArguments ( argc, argv );
	
	bzero ( &samArgs, sizeof ( samArgs ) );
	bzero ( &atapiArgs, sizeof ( atapiArgs ) );
	
	samArgs.type 		= kSAMTypeDebug;
	samArgs.operation 	= kSAMOperationGetFlags;
	samArgs.validBits 	= kSAMTraceFlagsValidMask;
	
	error = sysctlbyname ( SAM_SYSCTL, NULL, NULL, &samArgs, sizeof ( samArgs ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to get old samtrace flags\n" );
	}

	gSavedSAMTraceMask = samArgs.samTraceFlags;
	
	atapiArgs.type 		= kATAPITypeDebug;
	atapiArgs.operation = kATAPIOperationGetFlags;
	
	error = sysctlbyname ( ATAPI_SYSCTL, NULL, NULL, &atapiArgs, sizeof ( atapiArgs ) );
	if ( error != 0 )
	{
		
		LoadATAPIExtension ( );
		
		error = sysctlbyname ( ATAPI_SYSCTL, NULL, NULL, &atapiArgs, sizeof ( atapiArgs ) );
		if ( error != 0 )
		{
			fprintf ( stderr, "sysctlbyname failed to get old atapi debug flags second time\n" );
		}
		
	}
	
	gSavedATAPIDebugMask = atapiArgs.debugFlags;	
	
	samArgs.type 			= kSAMTypeDebug;
	samArgs.operation 		= kSAMOperationSetFlags;
	samArgs.validBits 		= kSAMTraceFlagsValidMask;
	samArgs.samDebugFlags 	= 0;
	samArgs.samTraceFlags 	= gPrintfMask;
	
	error = sysctlbyname ( SAM_SYSCTL, NULL, NULL, &samArgs, sizeof ( samArgs ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to set new samtrace flags\n" );
	}
	
	atapiArgs.type 			= kATAPITypeDebug;
	atapiArgs.operation 	= kATAPIOperationSetFlags;
	atapiArgs.debugFlags 	= 1;
	
	error = sysctlbyname ( ATAPI_SYSCTL, NULL, NULL, &atapiArgs, sizeof ( atapiArgs ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to set new atapi debug flags\n" );
	}
	
#if DEBUG
	printf ( "gSavedSAMTraceMask = 0x%08X\n", gSavedSAMTraceMask );
	printf ( "gSavedATAPIDebugMask = 0x%08X\n", gSavedATAPIDebugMask );
	printf ( "gPrintfMask = 0x%08X\n", gPrintfMask );
	printf ( "gVerbose = %s\n", gVerbose == TRUE ? "True" : "False" );
	fflush ( stdout );
#endif
	
	// Set up signal handlers.
	RegisterSignalHandlers ( );	
	
	// Allocate trace buffer.
	AllocateTraceBuffer ( );
	
	// Remove the trace buffer.
	RemoveTraceBuffer ( );
	
	// Set the new trace buffer size.
	SetTraceBufferSize ( kTraceBufferSampleSize );
	
	// Initialize the trace buffer.
	InitializeTraceBuffer ( );
	
	// Enable the trace buffer.
	EnableTraceBuffer ( 1 );
	
	// Get the clock divisor.
	GetDivisor ( );
	
	// Does the user only want the trace points enabled and no logging?
	if ( gEnableTraceOnly == FALSE )
	{
		
		printf ( "ATAPILogger v1.0\n" );
		
		// No, they want logging. Start main loop.
		while ( 1 )
		{
			
			usleep ( 20 * kMicrosecondsPerMillisecond );
			CollectTrace ( );
			
		}
		
	}
	
	return 0;
	
}


//-----------------------------------------------------------------------------
//	PrintUsage
//-----------------------------------------------------------------------------

static void
PrintUsage ( void )
{
	
	printf ( "\n" );
	printf ( "Usage: %s [--help] [--enable] [--disable] [--scsitask] [--all] [--verbose]\n", gProgramName );
	printf ( "\n" );
	
	exit ( 0 );
	
}


//-----------------------------------------------------------------------------
//	ParseArguments
//-----------------------------------------------------------------------------

static void
ParseArguments ( int argc, const char * argv[] )
{
	
	int 					c;
	struct option 			long_options[] =
	{
		{ "all",			no_argument,	0, 'a' },
		{ "enable",			no_argument,	0, 'e' },
		{ "disable",		no_argument,	0, 'd' },
		{ "scsitask",		no_argument,	0, 's' },
		{ "verbose",		no_argument,	0, 'v' },
		{ "help",			no_argument,	0, 'h' },
		{ 0, 0, 0, 0 }
	};
	
	// If no args specified, enable firewire sbp2 driver logging only...
	if ( argc == 1 )
	{
		return;	
	}
	
    while ( ( c = getopt_long ( argc, ( char * const * ) argv , "asptlgbrmvh?", long_options, NULL  ) ) != -1 )
	{
		
		switch ( c )
		{
			
			case 'a':
				gPrintfMask = ~0;
				break;
			
			case 'e':
				gEnableTraceOnly = TRUE;
				break;
			
			case 'd':
				gSavedSAMTraceMask 	= 0;
				gSavedATAPIDebugMask	= 0;
				gSetRemoveFlag = FALSE;
				Quit ( "Quit via user-specified trace disable\n" );
				break;
			
			case 's':
				gPrintfMask |= (1 << kSAMClassSCSITask);
				break;
			
			case 'v':
				gVerbose = TRUE;
				break;
			
			case 'h':
				PrintUsage ( );
				break;
				
			default:
				break;
			
		}
		
	}
	
}


//-----------------------------------------------------------------------------
//	RegisterSignalHandlers
//-----------------------------------------------------------------------------

static void
RegisterSignalHandlers ( void )
{
	
	signal ( SIGINT, SignalHandler );
	signal ( SIGQUIT, SignalHandler );
	signal ( SIGHUP, SignalHandler );
	signal ( SIGTERM, SignalHandler );
	
}


//-----------------------------------------------------------------------------
//	AllocateTraceBuffer
//-----------------------------------------------------------------------------

static void
AllocateTraceBuffer ( void )
{
	
	gTraceBuffer = ( kd_buf * ) malloc ( kTraceBufferSampleSize * sizeof ( kd_buf ) );
	if ( gTraceBuffer == NULL )
	{
		Quit ( "Can't allocate memory for tracing info\n" );
	}
	
}


//-----------------------------------------------------------------------------
//	SignalHandler
//-----------------------------------------------------------------------------

static void
SignalHandler ( int signal )
{

#pragma unused ( signal )
	
	EnableTraceBuffer ( 0 );
	RemoveTraceBuffer ( );
	ResetDebugFlags ( );
	
	exit ( 0 );
	
}


//-----------------------------------------------------------------------------
//	EnableTraceBuffer
//-----------------------------------------------------------------------------

static void
EnableTraceBuffer ( int val ) 
{
	
	int 		mib[6];
	size_t 		needed;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDENABLE;		/* protocol */
	mib[3] = val;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 4, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDENABLE\n" );
	
	if ( val )
		gTraceEnabled = TRUE;
	else
		gTraceEnabled = FALSE;
	
}


//-----------------------------------------------------------------------------
//	SetTraceBufferSize
//-----------------------------------------------------------------------------

static void
SetTraceBufferSize ( int nbufs ) 
{
	
	int 		mib[6];
	size_t 		needed;

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETBUF;
	mib[3] = nbufs;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 4, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETBUF\n" );

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETUP\n" );
	
}


//-----------------------------------------------------------------------------
//	GetTraceBufferInfo
//-----------------------------------------------------------------------------

static void
GetTraceBufferInfo ( kbufinfo_t * val )
{
	
	int 		mib[6];
	size_t 		needed;

	needed = sizeof ( *val );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDGETBUF;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, val, &needed, 0, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDGETBUF\n" );
	
}


//-----------------------------------------------------------------------------
//	RemoveTraceBuffer
//-----------------------------------------------------------------------------

static void
RemoveTraceBuffer ( void ) 
{
	
	int 		mib[6];
	size_t 		needed;
	
	errno = 0;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;		/* protocol */
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, NULL, &needed, NULL, 0 ) < 0 )
	{
		
		gSetRemoveFlag = FALSE;
		
		if ( errno == EBUSY )
			Quit ( "The trace facility is currently in use...\n    fs_usage, sc_usage, and latency use this feature.\n\n" );
		
		else
			Quit ( "Trace facility failure, KERN_KDREMOVE\n" );
		
	}
	
}


//-----------------------------------------------------------------------------
//	InitializeTraceBuffer
//-----------------------------------------------------------------------------

static void
InitializeTraceBuffer ( void ) 
{

	int 		mib[6];
	size_t 		needed;
	kd_regtype	kr;
	
	kr.type 	= KDBG_RANGETYPE;
	kr.value1 	= 0;
	kr.value2	= -1;
	
	needed = sizeof ( kd_regtype );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, &kr, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETREG\n" );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETUP\n" );
	
}


//-----------------------------------------------------------------------------
//	CollectTrace
//-----------------------------------------------------------------------------

static void
CollectTrace ( void )
{
	
	int				mib[6];
	int 			index;
	int				count;
	size_t 			needed;
	kbufinfo_t 		bufinfo = { 0, 0, 0, 0, 0 };
	
	/* Get kernel buffer information */
	GetTraceBufferInfo ( &bufinfo );
	
	needed = bufinfo.nkdbufs * sizeof ( kd_buf );
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	
	if ( sysctl ( mib, 3, gTraceBuffer, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDREADTR\n" );
	
	count = needed;
	
	if ( bufinfo.flags & KDBG_WRAPPED )
	{
		
		EnableTraceBuffer ( 0 );
		EnableTraceBuffer ( 1 );
		
	}
	
	for ( index = 0; index < count; index++ )
	{
		
		int 				debugID;
		int 				type;
		uint64_t 			now;
		int64_t 			usecs;
		int 				secs;
		time_t 				currentTime;
		
		debugID = gTraceBuffer[index].debugid;
		type	= debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
		
		now = gTraceBuffer[index].timestamp & KDBG_TIMESTAMP_MASK;
		
		if ( index == 0 )
		{
			
			/*
			 * Compute bias seconds after each trace buffer read.
			 * This helps resync timestamps with the system clock
			 * in the event of a system sleep.
			 */
			usecs = ( int64_t )( now / gDivisor );
			secs = usecs / kMicrosecondsPerSecond;
			currentTime = time ( NULL );
			gBiasSeconds = currentTime - secs;
			
		}
		
		switch ( type )
		{
			
			case kSCSITaskTracePointCDBLog1Code:
			{
				
				SCSITaskLogEntry *	entry = NULL;
				
				// If this isn't asked for, don't do any work.
				if ( gPrintfMask & ( 1 << kSAMClassSCSITask ) == 0 )
					continue;
				
				entry = TAILQ_FIRST ( &gListHead );
				while ( entry != NULL )
				{
					
					if ( entry->taskID == gTraceBuffer[index].arg2 )
					{
						
						break;
						
					}
					
					entry = TAILQ_NEXT ( entry, chain );
					
				}
				
				if ( entry == NULL )
				{
					
					entry = ( SCSITaskLogEntry * ) malloc ( sizeof ( SCSITaskLogEntry ) );
					TAILQ_INSERT_TAIL ( &gListHead, entry, chain );
					
				}
				
				bzero ( entry->cdb, sizeof ( entry->cdb ) );
				
				entry->taskID = gTraceBuffer[index].arg2;
				entry->cdb[0] = gTraceBuffer[index].arg3 & 0xFF;
				entry->cdb[1] = ( gTraceBuffer[index].arg3 >>  8 ) & 0xFF;
				entry->cdb[2] = ( gTraceBuffer[index].arg3 >> 16 ) & 0xFF;
				entry->cdb[3] = ( gTraceBuffer[index].arg3 >> 24 ) & 0xFF;
				entry->cdb[4] = gTraceBuffer[index].arg4 & 0xFF;
				entry->cdb[5] = ( gTraceBuffer[index].arg4 >>  8 ) & 0xFF;
				entry->cdb[6] = ( gTraceBuffer[index].arg4 >> 16 ) & 0xFF;
				entry->cdb[7] = ( gTraceBuffer[index].arg4 >> 24 ) & 0xFF;
				
			}
			break;

			case kSCSITaskTracePointCDBLog2Code:
			{
				
				// If this isn't asked for, don't do any work.
				if ( gPrintfMask & ( 1 << kSAMClassSCSITask ) == 0 )
					continue;
				
				if ( !TAILQ_EMPTY ( &gListHead ) )
				{
					
					SCSITaskLogEntry *	entry = NULL;
					
					entry = TAILQ_FIRST ( &gListHead );
					while ( entry != NULL )
					{
						
						if ( entry->taskID == gTraceBuffer[index].arg2 )
						{
							
							entry->cdb[ 8] = gTraceBuffer[index].arg3 & 0xFF;
							entry->cdb[ 9] = ( gTraceBuffer[index].arg3 >>  8 ) & 0xFF;
							entry->cdb[10] = ( gTraceBuffer[index].arg3 >> 16 ) & 0xFF;
							entry->cdb[11] = ( gTraceBuffer[index].arg3 >> 24 ) & 0xFF;
							entry->cdb[12] = gTraceBuffer[index].arg4 & 0xFF;
							entry->cdb[13] = ( gTraceBuffer[index].arg4 >>  8 ) & 0xFF;
							entry->cdb[14] = ( gTraceBuffer[index].arg4 >> 16 ) & 0xFF;
							entry->cdb[15] = ( gTraceBuffer[index].arg4 >> 24 ) & 0xFF;
							
							printf ( "%-8.8s Request: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
								&( ctime ( &currentTime )[11] ),
								entry->cdb[ 0], entry->cdb[ 1], entry->cdb[ 2], entry->cdb[ 3],
								entry->cdb[ 4], entry->cdb[ 5], entry->cdb[ 6], entry->cdb[ 7],
								entry->cdb[ 8], entry->cdb[ 9], entry->cdb[10], entry->cdb[11],
								entry->cdb[12], entry->cdb[13], entry->cdb[14], entry->cdb[15] );
							
						}
						
						entry = TAILQ_NEXT ( entry, chain );
						
					}
					
				}
				
			}
			break;

			case kSCSITaskTracePointResponseLog1Code:
			{

				// If this isn't asked for, don't do any work.
				if ( gPrintfMask & ( 1 << kSAMClassSCSITask ) == 0 )
					continue;
				
				if ( !TAILQ_EMPTY ( &gListHead ) )
				{
					
					SCSITaskLogEntry *	entry = NULL;
					
					entry = TAILQ_FIRST ( &gListHead );
					while ( entry != NULL )
					{
						
						if ( entry->taskID == gTraceBuffer[index].arg2 )
						{

							printf ( "%-8.8s Response: serviceResponse = 0x%02X, taskStatus = 0x%02X\n",
									 &( ctime ( &currentTime )[11] ),
									 ( unsigned int ) gTraceBuffer[index].arg3 & 0xFF,
									 ( unsigned int ) ( gTraceBuffer[index].arg3 >> 8 ) & 0xFF );
							
							break;
							
						}
						
						entry = TAILQ_NEXT ( entry, chain );
						
					}
					
				}
				
			}
			break;

			case kSCSITaskTracePointResponseLog2Code:
			{

				// If this isn't asked for, don't do any work.
				if ( gPrintfMask & ( 1 << kSAMClassSCSITask ) == 0 )
					continue;

				fflush ( stdout );
				
				if ( !TAILQ_EMPTY ( &gListHead ) )
				{
					
					SCSITaskLogEntry *	entry = NULL;
					
					entry = TAILQ_FIRST ( &gListHead );
					while ( entry != NULL )
					{
						
						if ( entry->taskID == gTraceBuffer[index].arg2 )
						{
							
							if ( gTraceBuffer[index].arg4 & kSenseDataValidMask )
							{
								
								printf ( "%-8.8s Response: serviceResponse = %d, taskStatus = %d, senseKey = %d, ASC = 0x%02X, ASCQ = 0x%02X\n",
										 &( ctime ( &currentTime )[11] ),
										 ( unsigned int ) gTraceBuffer[index].arg3 & 0xFF,
										 ( unsigned int ) ( gTraceBuffer[index].arg3 >> 8 ) & 0xFF,
										 ( unsigned int ) gTraceBuffer[index].arg4 & 0xFF,
										 ( unsigned int ) ( gTraceBuffer[index].arg4 >> 8 ) & 0xFF,
										 ( unsigned int ) ( gTraceBuffer[index].arg4 >> 16 ) & 0xFF );
								
							}
							
							else
							{
								
								printf ( "%-8.8s Response: serviceResponse = 0x%02X, taskStatus = 0x%02X\n",
										 &( ctime ( &currentTime )[11] ),
										 ( unsigned int ) gTraceBuffer[index].arg3 & 0xFF,
										 ( unsigned int ) ( gTraceBuffer[index].arg3 >> 8 ) & 0xFF );
								
							}
							
							break;
							
						}
						
						entry = TAILQ_NEXT ( entry, chain );
						
					}
					
					if ( entry != NULL )
					{
						
						TAILQ_REMOVE ( &gListHead, entry, chain );
						free ( entry );
						
					}
					
				}
				
			}
			break;

			case kATADeviceInfoCode:
			{
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI start, obj = 0x%08X, UnitID = 0x%08X, Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
			}
			break;
			
			case kATASendSCSICommandCode:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Send SCSI Command, obj = 0x%08X, UnitID = 0x%08X, cmd = 0x%08X, SCSITaskIdentifier = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3,
						 ( unsigned int ) gTraceBuffer[index].arg4 );
				
			}
			break;

			case kATASendSCSICommandFailedCode:
			{
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Send SCSI Command failed!, obj = 0x%08X, UnitID = 0x%08X, SCSITaskIdentifier = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
			}
			break;

			case kATACompleteSCSICommandCode:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Complete SCSI Command, obj = 0x%08X, UnitID = 0x%08X, SCSITaskIdentifier = 0x%08X, serviceResponse = 0x%02X, taskStatus = 0x%02X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3,
						 ( unsigned int ) ( gTraceBuffer[index].arg4 >> 8 ) & 0xFF,
						 ( unsigned int ) gTraceBuffer[index].arg4 & 0xFF );
				
			}
			break;

			case kATAAbortCode:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Abort, obj = 0x%08X, UnitID = 0x%08X, SCSITaskIdentifier = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;

			case kATAPowerOnResetCode:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Resetting device at Handle Power On, obj = 0x%08X, UnitID = 0x%08X Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;

			case kATAPowerOnNoResetCode:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Not resetting device at Handle Power On, obj = 0x%08X, UnitID = 0x%08X Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;

			case kATAHandlePowerOffCode:
			{			

				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Handle Power Off, obj = 0x%08X, UnitID = 0x%08X Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );

			}
			break;

			case kATAResetCompleteCode:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Reset complete, UnitID = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1 );
				
			}
			break;

			case kATAResetCode:
			{

				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Device Reset, obj = 0x%08X, UnitID = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2 );

			}
			break;
			
			case kATADriverPowerOff:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Driver Power Off, obj = 0x%08X, UnitID = 0x%08X, cmd = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;
			
			case kATAStartStatusPolling:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Start polling Status Register, obj = 0x%08X, UnitID = 0x%08X, Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
						
			}
			break;
			
			case kATAStatusPoll:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Polling Status Register, obj = 0x%08X, UnitID = 0x%08X, Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
						
			}
			break;
			
			case kATAStopStatusPolling:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Stop polling Status Register, obj = 0x%08X, UnitID = 0x%08X, Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
						
			}
			break;									

			case kATASendATASleepCmd:
			{
				
				printf ( "%-8.8s ", &( ctime ( &currentTime )[11] ) );
				printf ( "ATAPI Send ATA Sleep Command, obj = 0x%08X, UnitID = 0x%08X, cmd = Device Type = 0x%08X\n",
						 ( unsigned int ) gTraceBuffer[index].arg1,
						 ( unsigned int ) gTraceBuffer[index].arg2,
						 ( unsigned int ) gTraceBuffer[index].arg3 );
						
			}
			break;
			
			default:
			{
				continue;
			}
			break;
			
		}
		
	}
	
	fflush ( 0 );
	
}


//-----------------------------------------------------------------------------
//	LoadATAPIExtension
//-----------------------------------------------------------------------------

static void
LoadATAPIExtension ( void )
{
	
	posix_spawn_file_actions_t	fileActions;
	char			argv0 []	= "/sbin/kextload";
	char			argv1 []	= "/System/Library/Extensions/IOATAFamily.kext/Contents/PlugIns/IOATAPIProtocolTransport.kext";
	char * const	argv[]		= { argv0, argv1, NULL };
	char * const	env[]		= { NULL };
	pid_t			child		= 0;
	union wait 		status;
	
	posix_spawn_file_actions_init ( &fileActions );
	posix_spawn_file_actions_addclose ( &fileActions, STDOUT_FILENO );
	posix_spawn_file_actions_addclose ( &fileActions, STDERR_FILENO );
	
	posix_spawn ( &child, "/sbin/kextload", &fileActions, NULL, argv, env );
	
	if ( !( ( wait4 ( child, ( int * ) &status, 0, NULL ) == child ) && ( WIFEXITED ( status ) ) ) )
	{
		printf ( "Error loading ATAPI extension\n" );
	}	
	
	posix_spawn_file_actions_destroy ( &fileActions );
	
}


//-----------------------------------------------------------------------------
//	Quit
//-----------------------------------------------------------------------------

static void
Quit ( const char * s )
{
	
	if ( gTraceEnabled == TRUE )
		EnableTraceBuffer ( 0 );
	
	if ( gSetRemoveFlag == TRUE )
		RemoveTraceBuffer ( );
	
	ResetDebugFlags ( );
	
	fprintf ( stderr, "%s: ", gProgramName );
	if ( s != NULL )
	{
		fprintf ( stderr, "%s", s );
	}
	
	fprintf ( stderr, "\n" );
	
	exit ( 1 );
	
}


//-----------------------------------------------------------------------------
//	ResetDebugFlags
//-----------------------------------------------------------------------------

static void
ResetDebugFlags ( void )
{
	
	SAMSysctlArgs	samArgs;
	ATAPISysctlArgs	atapiArgs;
	int				error;	
	
	samArgs.type 			= kSAMTypeDebug;
	samArgs.operation 		= kSAMOperationSetFlags;
	samArgs.validBits 		= kSAMTraceFlagsValidMask;
	samArgs.samDebugFlags 	= 0;
	samArgs.samTraceFlags 	= gSavedSAMTraceMask;
	
	error = sysctlbyname ( SAM_SYSCTL, NULL, NULL, &samArgs, sizeof ( samArgs ) );
	
	atapiArgs.type 			= kATAPITypeDebug;
	atapiArgs.operation		= kATAPIOperationSetFlags;
	atapiArgs.debugFlags 	= gSavedATAPIDebugMask;
	
	error = sysctlbyname ( ATAPI_SYSCTL, NULL, NULL, &atapiArgs, sizeof ( atapiArgs ) );
	
}


//-----------------------------------------------------------------------------
//	GetDivisor
//-----------------------------------------------------------------------------

static void
GetDivisor ( void )
{
	
	struct mach_timebase_info	mti;
	
	mach_timebase_info ( &mti );
	
	gDivisor = ( ( double ) mti.denom / ( double ) mti.numer) * 1000;
	
}
