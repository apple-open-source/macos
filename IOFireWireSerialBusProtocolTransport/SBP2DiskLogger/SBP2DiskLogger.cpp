/*
 * Copyright (c) 2008-2015 Apple Inc. All rights reserved.
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

/*
g++ -W -Wall -I/System/Library/Frameworks/System.framework/PrivateHeaders -I/System/Library/Frameworks/Kernel.framework/PrivateHeaders -DPRIVATE -D__APPLE_PRIVATE -O -arch ppc -arch i386 -arch x86_64 -o SBP2DiskLogger SBP2DiskLogger.cpp
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

#include "../IOFireWireSerialBusProtocolTransportTimestamps.h"
#include "../IOFireWireSerialBusProtocolTransportDebugging.h"
#include <IOKit/scsi/SCSICommandOperationCodes.h>

#define DEBUG 			0


//-----------------------------------------------------------------------------
//	Structures
//-----------------------------------------------------------------------------

typedef struct SCSITaskLogEntry
{
	TAILQ_ENTRY(SCSITaskLogEntry)	chain;
	unsigned int					taskID;
	uint8_t							cdb[16];
	uint8_t							senseKey;
	uint8_t							ASC;
	uint8_t							ASCQ;
} SCSITaskLogEntry;


typedef struct FireWireDevice
{
	TAILQ_ENTRY(FireWireDevice)		chain;
	unsigned int					obj;
	uint64_t						GUID;
} FireWireDevice;


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

enum
{
	kGUIDCode								= FW_TRACE ( kGUID ),
	kLoginRequestCode						= FW_TRACE ( kLoginRequest ),
	kLoginCompletionCode					= FW_TRACE ( kLoginCompletion ),
	kLoginLostCode							= FW_TRACE ( kLoginLost ),
	kLoginResumedCode						= FW_TRACE ( kLoginResumed ),
	kSendSCSICommandCode1					= FW_TRACE ( kSendSCSICommand1 ),
	kSendSCSICommandCode2					= FW_TRACE ( kSendSCSICommand2 ),
	kSCSICommandSenseDataCode				= FW_TRACE ( kSCSICommandSenseData ),
	kCompleteSCSICommandCode				= FW_TRACE ( kCompleteSCSICommand ),
	kSubmitOrbCode							= FW_TRACE ( kSubmitOrb ),
	kStatusNotifyCode						= FW_TRACE ( kStatusNotify ),
	kFetchAgentResetCode					= FW_TRACE ( kFetchAgentReset ),
	kFetchAgentResetCompleteCode			= FW_TRACE ( kFetchAgentResetComplete ),
	kLogicalUnitResetCode					= FW_TRACE ( kLogicalUnitReset ),
	kLogicalUnitResetCompleteCode			= FW_TRACE ( kLogicalUnitResetComplete )
};


#define kTraceBufferSampleSize			60000
#define kMicrosecondsPerSecond			1000000
#define kMicrosecondsPerMillisecond		1000
#define kFilePathMaxSize				256
#define kInvalid						0xdeadbeef
#define kDivisorEntry					0xfeedface


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

int					gBiasSeconds                        = 0;
int					gNumCPUs                            = 1;
double				gDivisor                            = 0.0;		/* Trace divisor converts to microseconds */
kd_buf *			gTraceBuffer                        = NULL;
boolean_t			gTraceEnabled                       = FALSE;
boolean_t			gSetRemoveFlag                      = TRUE;
boolean_t			gVerbose                            = FALSE;
boolean_t			gEnableTraceOnly                    = FALSE;
const char *		gProgramName                        = NULL;
uint32_t			gPrintfMask                         = 0;
uint32_t			gSavedFWDebugMask                   = 0;
boolean_t			gReadTraceFile                      = FALSE;
boolean_t			gWriteToTraceFile                   = FALSE;
FILE *				gTraceFileStream                    = NULL;
char				gTraceFilePath [ kFilePathMaxSize ] = { 0 };

u_int8_t			fullCDB [ 16 ]				= { 0 };
int64_t 			current_usecs				= 0;
int64_t 			prev_usecs					= 0;
int64_t				delta_usecs					= 0;

TAILQ_HEAD(SCSITaskLogEntryHead, SCSITaskLogEntry) gListHead	= TAILQ_HEAD_INITIALIZER(gListHead);
TAILQ_HEAD(FireWireDeviceHead, FireWireDevice) gDeviceListHead	= TAILQ_HEAD_INITIALIZER(gDeviceListHead);


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
ParseTraceFile ( void );

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
ParseKernelTracePoint ( kd_buf inTracePoint );

static void
ParseArguments ( int argc, const char * argv[] );

static void
PrintUsage ( void );

static void
PrintSCSICommand ( void );

static void
PrintTimeStamp ( void );

static FireWireDevice *
MapObjectToDevice ( unsigned int obj );

static uint64_t
MapObjectToGUID ( unsigned int obj );

static void
LoadFireWireExtension ( void );

static void
ResetDebugFlags ( void );


//-----------------------------------------------------------------------------
//	Main
//-----------------------------------------------------------------------------

int
main ( int argc, const char * argv[] )
{
	
	FWSysctlArgs	fwArgs;
	int				error;
	
	gProgramName = argv[0];
	
	if ( geteuid ( ) != 0 )
	{
		
		fprintf ( stderr, "'%s' must be run as root...\n", gProgramName );
		exit ( 1 );
		
	}
	
	if ( reexec_to_match_kernel() )
	{
		fprintf( stderr, "Could not re-execute to match kernel architecture. (Error %d)\n", errno );
		exit( 1 );
    }		
	
	// Get program arguments.
	ParseArguments ( argc, argv );
	
	bzero ( &fwArgs, sizeof ( fwArgs ) );
		
	fwArgs.type 		= kFWTypeDebug;
	fwArgs.operation 	= kFWOperationGetFlags;
	
	error = sysctlbyname ( FWSBP_SYSCTL, NULL, NULL, &fwArgs, sizeof ( fwArgs ) );
	if ( error != 0 )
	{
		
		LoadFireWireExtension ( );
		
		error = sysctlbyname ( FWSBP_SYSCTL, NULL, NULL, &fwArgs, sizeof ( fwArgs ) );
		if ( error != 0 )
		{
			fprintf ( stderr, "sysctlbyname failed to get old fw debug flags second time\n" );
		}
		
	}
	
	gSavedFWDebugMask = fwArgs.debugFlags;	
	
	fwArgs.type 		= kFWTypeDebug;
	fwArgs.operation 	= kFWOperationSetFlags;
	fwArgs.debugFlags 	= fwArgs.debugFlags | kSBP2DiskEnableTracePointsMask;
	
	error = sysctlbyname ( FWSBP_SYSCTL, NULL, NULL, &fwArgs, sizeof ( fwArgs ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to set new fw debug flags\n" );
	}
	
#if DEBUG
	printf ( "gSavedFWDebugMask = 0x%08X\n", gSavedFWDebugMask );
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
        
        printf ( "SBP2DiskLogger v1.1\n" );
        
        if ( gReadTraceFile == FALSE )
        {
            
            // No, they want logging. Start main loop.
            while ( 1 )
            {
                
                usleep ( 20 * kMicrosecondsPerMillisecond );
                CollectTrace ( );
                
            }
            
        }
        
        else
        {
            
            ParseTraceFile ( );
            
        }
        
    }
	
	return 0;
	
}


//-----------------------------------------------------------------------------
//	ParseTraceFile
//-----------------------------------------------------------------------------

static void
ParseTraceFile ( )
{
    
    FILE * traceFile;
    
    traceFile = fopen ( gTraceFilePath, "r" );
    kd_buf kp;
    bzero( &kp, sizeof ( kd_buf ) );
    
    if ( traceFile )
    {
        
        while ( fread ( &kp, sizeof ( kd_buf ), 1, traceFile ) )
        {
            
            kd_buf tracepoint;
            bzero ( &tracepoint, sizeof ( kd_buf ) );
            
            if ( kp.debugid == kInvalid )
            {
                
                printf ( "Found an invalid entry in raw file.\n" );
                continue;
                
            }
            
            if ( kp.debugid == kDivisorEntry )
            {
                
                gDivisor = ( double )( kp.timestamp );
                printf ( "Found divisor %f as 0x%llx\n", gDivisor, kp.timestamp );
                
            }
            else
            {
                
                // send tracepoint to be processed
                ParseKernelTracePoint ( kp );
                
            }
            
        }
        
        fclose ( traceFile );
        
    }
    
    else
    {
        Quit ( "Could not open specified trace file :(\n" );
    }
    
}


//-----------------------------------------------------------------------------
//	PrintUsage
//-----------------------------------------------------------------------------

static void
PrintUsage ( void )
{
	
	printf ( "\n" );
	printf ( "Usage: %s [--help] [--enable] [--disable] [--all] [--verbose] [--output <file_path>] [--read <file_path>]\n", gProgramName );
	printf ( "\n" );
	
	exit ( 0 );
	
}


//-----------------------------------------------------------------------------
//	PrintSCSICommand
//-----------------------------------------------------------------------------

static void
PrintSCSICommand ( void )
{
    
    switch ( fullCDB [0] )
    {
            
        case kSCSICmd_TEST_UNIT_READY:
        {
            
            printf ( " kSCSICmd_TEST_UNIT_READY\n" );
            
        }
            break;
            
        case kSCSICmd_REQUEST_SENSE:
        {
            
            printf ( " kSCSICmd_REQUEST_SENSE\n" );
            
        }
            break;
            
        case kSCSICmd_READ_10:
        {
            
            u_int32_t	LOGICAL_BLOCK_ADDRESS	= 0;
            u_int16_t	TRANSFER_LENGTH			= 0;
            
            LOGICAL_BLOCK_ADDRESS   = fullCDB [2];
            LOGICAL_BLOCK_ADDRESS <<= 8;
            LOGICAL_BLOCK_ADDRESS  |= fullCDB [3];
            LOGICAL_BLOCK_ADDRESS <<= 8;
            LOGICAL_BLOCK_ADDRESS  |= fullCDB [4];
            LOGICAL_BLOCK_ADDRESS <<= 8;
            LOGICAL_BLOCK_ADDRESS  |= fullCDB [5];
            
            TRANSFER_LENGTH   = fullCDB [7];
            TRANSFER_LENGTH <<= 8;
            TRANSFER_LENGTH  |= fullCDB [8];
            
            printf ( "kSCSICmd_READ_10, LBA = %p, length = %p\n", ( void * ) LOGICAL_BLOCK_ADDRESS, ( void * ) TRANSFER_LENGTH );
            
        }
            break;
            
        case kSCSICmd_WRITE_10:
        {
            
            u_int32_t	LOGICAL_BLOCK_ADDRESS	= 0;
            u_int16_t	TRANSFER_LENGTH			= 0;
            
            LOGICAL_BLOCK_ADDRESS   = fullCDB [2];
            LOGICAL_BLOCK_ADDRESS <<= 8;
            LOGICAL_BLOCK_ADDRESS  |= fullCDB [3];
            LOGICAL_BLOCK_ADDRESS <<= 8;
            LOGICAL_BLOCK_ADDRESS  |= fullCDB [4];
            LOGICAL_BLOCK_ADDRESS <<= 8;
            LOGICAL_BLOCK_ADDRESS  |= fullCDB [5];
            
            TRANSFER_LENGTH   = fullCDB [7];
            TRANSFER_LENGTH <<= 8;
            TRANSFER_LENGTH  |= fullCDB [8];
            
            printf ( "kSCSICmd_WRITE_10, LBA = %p, length = %p\n", ( void * ) LOGICAL_BLOCK_ADDRESS, ( void * ) TRANSFER_LENGTH );
            
        }
            break;
            
        default:
        {
            printf ( "This command has not yet been decoded\n" );
        }
            break;
            
    }
    
}


//-----------------------------------------------------------------------------
//	PrintTimeStamp
//-----------------------------------------------------------------------------

static void
PrintTimeStamp ( void )
{
    
    time_t		currentTime = time ( NULL );
    
    if ( prev_usecs == 0 )
    {
        delta_usecs = 0;
    }
    else
    {
        delta_usecs = current_usecs - prev_usecs;
    }
    
    prev_usecs = current_usecs;
    
    printf ( "%-8.8s [%lld][%10lld us]", &( ctime ( &currentTime )[11] ), current_usecs, delta_usecs );
    
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
		{ "all",			no_argument,        0, 'a' },
		{ "enable",			no_argument,        0, 'e' },
		{ "disable",		no_argument,        0, 'd' },
		{ "verbose",		no_argument,        0, 'v' },
        { "output",         required_argument,  0, 'o' },
        { "read",           required_argument,  0, 'r' },
		{ "help",			no_argument,        0, 'h' },
		{ 0, 0, 0, 0 }
	};
	
	// If no args specified, enable firewire sbp2 driver logging only...
	if ( argc == 1 )
	{
		gVerbose = TRUE;
		return;	
	}
	
    while ( ( c = getopt_long ( argc, ( char * const * ) argv , "aedvo:r:h?", long_options, NULL  ) ) != -1 )
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
				gSavedFWDebugMask	= 0;
				gSetRemoveFlag 		= FALSE;
				Quit ( "Quit via user-specified trace disable\n" );
				break;
			
			case 'v':
				gVerbose = TRUE;
				break;
                
            case 'o':
                gWriteToTraceFile = TRUE;
                if ( optarg == NULL )
                {
                    Quit ( "No file specified with -f argument\n");
                }
                
                if ( strlcpy ( gTraceFilePath, optarg, sizeof ( gTraceFilePath ) ) >= sizeof ( gTraceFilePath ) )
                {
                    Quit ( "The path length of raw file is too long\n" );
                }
                break;
                
            case 'r':
                gReadTraceFile = TRUE;
                printf("input is from file\n");
                if ( optarg == NULL )
                {
                    Quit ( "No file specified with -r argument\n");
                }
                
                if ( strlcpy ( gTraceFilePath, optarg, sizeof ( gTraceFilePath ) ) >= sizeof ( gTraceFilePath ) )
                {
                    Quit ( "The path length of raw file is too long\n" );
                }
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
	
	size_t	len;
	int		mib[3];
	
	// grab the number of cpus
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	mib[2] = 0;
	
	len = sizeof ( gNumCPUs );
	
	sysctl ( mib, 2, &gNumCPUs, &len, NULL, 0 );
	
	gTraceBuffer = ( kd_buf * ) malloc ( gNumCPUs * kTraceBufferSampleSize * sizeof ( kd_buf ) );
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
        
        debugID = gTraceBuffer[index].debugid;
        type	= debugID & ~( DBG_FUNC_START | DBG_FUNC_END );
   
        if ( ( type >= 0x05278000 ) && ( type <= 0x052783FF ) )
        {
            
            // Print trace data to stdout.
            if ( gWriteToTraceFile == FALSE )
            {
            
                ParseKernelTracePoint ( gTraceBuffer[index] );
                
            }
            
            else
            {
                
                fwrite (    ( const void * ) & ( gTraceBuffer [ index ] ),
                        sizeof ( kd_buf ),
                        1,
                        gTraceFileStream );
                
                fflush (    gTraceFileStream );
                
            }
            
        }
		
	}
	
	fflush ( 0 );
	
}


//-----------------------------------------------------------------------------
//	MapObjectToDevice
//-----------------------------------------------------------------------------

static FireWireDevice *
MapObjectToDevice ( unsigned int obj )
{
	
	FireWireDevice *	device = NULL;
	
	if ( !TAILQ_EMPTY ( &gDeviceListHead ) )
	{
		
		device = TAILQ_FIRST ( &gDeviceListHead );
		while ( device != NULL )
		{
			
			if ( device->obj == obj )
			{
				
				return device;
				
			}
			
		}
	
	}
	
	return NULL;
	
}


//-----------------------------------------------------------------------------
//	MapObjectToGUID
//-----------------------------------------------------------------------------

static uint64_t
MapObjectToGUID ( unsigned int obj )
{
	
	uint64_t			GUID 	= 0;
	FireWireDevice *	device 	= NULL;
	
	device = MapObjectToDevice ( obj );
	GUID = device->GUID;
	
	return GUID;
	
}


//-----------------------------------------------------------------------------
//	LoadFireWireExtension
//-----------------------------------------------------------------------------

static void
LoadFireWireExtension ( void )
{
	
	posix_spawn_file_actions_t	fileActions;
	const char * const          argv[]	= { "/sbin/kextload", "/System/Library/Extensions/IOFireWireSerialBusProtocolTransport.kext", NULL };
    char * const                env[]	= { NULL };
	pid_t                       child	= 0;
	union wait                  status;
	
	posix_spawn_file_actions_init ( &fileActions );
	posix_spawn_file_actions_addclose ( &fileActions, STDOUT_FILENO );
	posix_spawn_file_actions_addclose ( &fileActions, STDERR_FILENO );
	
	posix_spawn ( &child, "/sbin/kextload", &fileActions, NULL, ( char * const * ) argv, env );
	
	if ( !( ( wait4 ( child, ( int * ) &status, 0, NULL ) == child ) && ( WIFEXITED ( status ) ) ) )
	{
		printf ( "Error loading FW extension\n" );
	}	
	
	posix_spawn_file_actions_destroy ( &fileActions );
	
}


//-----------------------------------------------------------------------------
//	ParseKernelTracePoint
//-----------------------------------------------------------------------------

static void
ParseKernelTracePoint ( kd_buf inTracePoint )
{
 
    int 				debugID;
    int 				type;
    uint64_t 			now;
    
    debugID = inTracePoint.debugid;
    type	= debugID & ~( DBG_FUNC_START | DBG_FUNC_END );
    
    now = inTracePoint.timestamp & KDBG_TIMESTAMP_MASK;
    current_usecs = ( int64_t )( now / gDivisor );
    
    // Filter out the traces that aren't ours.
    if ( ( type <= 0x05278000 ) || ( type >= 0x052783FF ) )
        return;
    
    PrintTimeStamp ( );
   
    switch ( type )
    {
            
        case kSendSCSICommandCode1:
        {
            
            SCSITaskLogEntry *	entry = NULL;
            
            // If this isn't asked for, don't do any work.
            if ( gVerbose == FALSE )
                return;
            
            entry = ( SCSITaskLogEntry * ) malloc ( sizeof ( SCSITaskLogEntry ) );
            TAILQ_INSERT_TAIL ( &gListHead, entry, chain );
            
            // Initialize the fields.
            bzero ( entry->cdb, sizeof ( entry->cdb ) );
            entry->senseKey = 0;
            entry->ASC		= 0;
            entry->ASCQ		= 0;
            
            entry->taskID = inTracePoint.arg2;
            entry->cdb[0] = inTracePoint.arg3 & 0xFF;
            entry->cdb[1] = ( inTracePoint.arg3 >>  8 ) & 0xFF;
            entry->cdb[2] = ( inTracePoint.arg3 >> 16 ) & 0xFF;
            entry->cdb[3] = ( inTracePoint.arg3 >> 24 ) & 0xFF;
            entry->cdb[4] = inTracePoint.arg4 & 0xFF;
            entry->cdb[5] = ( inTracePoint.arg4 >>  8 ) & 0xFF;
            entry->cdb[6] = ( inTracePoint.arg4 >> 16 ) & 0xFF;
            entry->cdb[7] = ( inTracePoint.arg4 >> 24 ) & 0xFF;
            
        }
            break;
            
        case kSendSCSICommandCode2:
        {
            
            // If this isn't asked for, don't do any work.
            if ( gVerbose == FALSE )
                return;
            
            if ( !TAILQ_EMPTY ( &gListHead ) )
            {
                
                SCSITaskLogEntry *	entry = NULL;
                
                entry = TAILQ_FIRST ( &gListHead );
                while ( entry != NULL )
                {
                    
                    if ( entry->taskID == inTracePoint.arg2 )
                    {
                        
                        entry->cdb[ 8] = inTracePoint.arg3 & 0xFF;
                        entry->cdb[ 9] = ( inTracePoint.arg3 >>  8 ) & 0xFF;
                        entry->cdb[10] = ( inTracePoint.arg3 >> 16 ) & 0xFF;
                        entry->cdb[11] = ( inTracePoint.arg3 >> 24 ) & 0xFF;
                        entry->cdb[12] = inTracePoint.arg4 & 0xFF;
                        entry->cdb[13] = ( inTracePoint.arg4 >>  8 ) & 0xFF;
                        entry->cdb[14] = ( inTracePoint.arg4 >> 16 ) & 0xFF;
                        entry->cdb[15] = ( inTracePoint.arg4 >> 24 ) & 0xFF;
                        
                        printf ( "FireWire Send SCSI Command, Request[0x%08X]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                                entry->taskID,
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
            
        case kSCSICommandSenseDataCode:
        {
            
            if ( gVerbose == FALSE )
                return;
            
            if ( !TAILQ_EMPTY ( &gListHead ) )
            {
                
                SCSITaskLogEntry *	entry = NULL;
                
                entry = TAILQ_FIRST ( &gListHead );
                while ( entry != NULL )
                {
                    
                    if ( entry->taskID == inTracePoint.arg2 )
                    {
                        
                        entry->senseKey = inTracePoint.arg3;
                        entry->ASC = ( inTracePoint.arg4 >> 8 ) & 0xFF;
                        entry->ASCQ = inTracePoint.arg4 & 0xFF;
                        break;
                        
                    }
                    
                    entry = TAILQ_NEXT ( entry, chain );
                    
                }
                
            }
            
        }
            break;
            
        case kCompleteSCSICommandCode:
        {
            
            if ( gVerbose == FALSE )
                return;
            
            if ( !TAILQ_EMPTY ( &gListHead ) )
            {
                
                SCSITaskLogEntry *	entry = NULL;
                
                entry = TAILQ_FIRST ( &gListHead );
                while ( entry != NULL )
                {
                    
                    if ( entry->taskID == inTracePoint.arg2 )
                    {
                        
                        printf ( "FireWire SCSI Response[0x%08X]: serviceResponse = %d, taskStatus = %d, senseKey = 0x%02X, ASC = 0x%02X, ASCQ = 0x%02X\n",
                                ( unsigned int ) inTracePoint.arg2, ( int ) (inTracePoint.arg3 >> 8) & 0xFF, ( int ) inTracePoint.arg3 & 0xFF, entry->senseKey, entry->ASC, entry->ASCQ );
                        
                        TAILQ_REMOVE ( &gListHead, entry, chain );
                        free ( entry );
                        break;
                        
                    }
                    
                    entry = TAILQ_NEXT ( entry, chain );
                    
                }
                
            }
            
        }
            break;
            
        case kGUIDCode:
        {
            
            uint64_t			GUID 	= 0;
            FireWireDevice *	device	= NULL;
            
            GUID = inTracePoint.arg2;
            GUID = ( GUID << 32 ) | inTracePoint.arg3;
            
            if ( inTracePoint.arg4 == 0 )
            {
                
                printf ( "[GUID %qd]: FireWire SBP2 Device appeared, obj = 0x%08X\n", GUID, ( unsigned int ) inTracePoint.arg1 );
                
                device = ( FireWireDevice * ) malloc ( sizeof ( FireWireDevice ) );
                if ( device != NULL )
                {
                    
                    device->obj 	= inTracePoint.arg1;
                    device->GUID	= GUID;
                    
                    TAILQ_INSERT_TAIL ( &gDeviceListHead, device, chain );
                    
                }
                
            }
            
            else
            {
                
                printf ( "[GUID %qd]: FireWire SBP2 Device removed, obj = 0x%08X\n", GUID, ( unsigned int ) inTracePoint.arg1 );
                
                if ( !TAILQ_EMPTY ( &gDeviceListHead ) )
                {
                    
                    FireWireDevice *	device = NULL;
                    
                    device = MapObjectToDevice ( inTracePoint.arg1 );
                    if ( device != NULL )
                    {
                        
                        TAILQ_REMOVE ( &gDeviceListHead, device, chain );
                        free ( device );
                        
                    }
                    
                }
                
            }
            
        }
            break;
            
        case kLoginRequestCode:
        {
            
            printf ( "FireWire Login Request, obj = 0x%08X, state = %d, count = %d\n", ( unsigned int ) inTracePoint.arg1, ( int ) inTracePoint.arg2, ( int ) inTracePoint.arg3 );
            
        }
            break;
            
        case kLoginCompletionCode:
        {
            
            // We only have a valid SBP2 login params block who's values we can display if the login completed successfully.
            if ( inTracePoint.arg2 == 0 )
            {
                printf ( "FireWire Login Completion, obj = 0x%08X, status = 0x%08X, details = 0x%02X, sbpStatus = 0x%02X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3, ( unsigned int ) inTracePoint.arg4 );
            }
            else
            {
                printf ( "FireWire Login Completion, obj = 0x%08X, status = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            }
            
        }
            break;
            
        case kLoginLostCode:
        {
            
            printf ( "FireWire Login Lost, obj = 0x%08X, state = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            
        }
            break;
            
        case kLoginResumedCode:
        {
            
            printf ( "FireWire Login Resumed, obj = 0x%08X\n", ( unsigned int ) inTracePoint.arg1 );
            
        }
            break;
            
        case kSubmitOrbCode:
        {
            
            printf ( "FireWire Submit ORB, obj = 0x%08X, ORB = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            
        }
            break;
            
        case kStatusNotifyCode:
        {
            
            printf ( "FireWire Status Notify, obj = 0x%08X, ORB = 0x%08X, notificationEvent = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3 );
            
        }
            break;
            
        case kFetchAgentResetCode:
        {
            
            printf ( "FireWire Reset Fetch Agent, obj = 0x%08X, ORB = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            
        }
            break;
            
        case kFetchAgentResetCompleteCode:
        {
            
            printf ( "FireWire Fetch Agent Reset Complete, obj = 0x%08X, ORB = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            
        }
            break;
            
        case kLogicalUnitResetCode:
        {
            
            printf ( "FireWire Logical Unit Reset, obj = 0x%08X, ORB = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2 );
            
        }
            break;
            
        case kLogicalUnitResetCompleteCode:
        {
            
            printf ( "FireWire Logical Unit Reset Complete, obj = 0x%08X, ORB = 0x%08X, status = 0x%08X\n", ( unsigned int ) inTracePoint.arg1, ( unsigned int ) inTracePoint.arg2, ( unsigned int ) inTracePoint.arg3 );
            
        }
            break;
            
        default:
        {
            return;
        }
        break;
            
    }

    
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
	
	FWSysctlArgs	fwArgs;
	int				error;	
	
	fwArgs.type 		= kFWTypeDebug;
	fwArgs.operation	= kFWOperationSetFlags;
	fwArgs.debugFlags 	= gSavedFWDebugMask;
	
	error = sysctlbyname ( FWSBP_SYSCTL, NULL, NULL, &fwArgs, sizeof ( fwArgs ) );
	
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
