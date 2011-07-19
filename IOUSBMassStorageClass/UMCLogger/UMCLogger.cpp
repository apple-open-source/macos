/*
 * Copyright (c) 2008-2009 Apple Inc. All rights reserved.
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
g++ -W -Wall -I/System/Library/Frameworks/System.framework/PrivateHeaders -I/System/Library/Frameworks/Kernel.framework/PrivateHeaders -lutil -DPRIVATE -D__APPLE_PRIVATE -O -arch ppc -arch i386 -arch x86_64 -o UMCLogger UMCLogger.cpp
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
#include <IOKit/scsi/SCSICommandOperationCodes.h>

#include "../IOUSBMassStorageClassTimestamps.h"

#include <IOKit/usb/USB.h>

#define DEBUG 			0

//-----------------------------------------------------------------------------
//	Structures
//-----------------------------------------------------------------------------

typedef struct ReturnCodeSpec
{
	unsigned int	returnCode;
	const char *	string;
} ReturnCodeSpec;


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

enum
{

	// Generic USB Storage					0x052D0000 - 0x052D03FF
	kAbortedTaskCode                        = UMC_TRACE ( kAbortedTask ),
	kCompleteSCSICommandCode				= UMC_TRACE ( kCompleteSCSICommand ),
	kNewCommandWhileTerminatingCode         = UMC_TRACE ( kNewCommandWhileTerminating ),
	kLUNConfigurationCompleteCode			= UMC_TRACE ( kLUNConfigurationComplete ),
	kIOUMCStorageCharacDictFoundCode		= UMC_TRACE ( kIOUMCStorageCharacDictFound ),
	kNoProtocolForDeviceCode				= UMC_TRACE ( kNoProtocolForDevice ),
	kIOUSBMassStorageClassStartCode         = UMC_TRACE ( kIOUSBMassStorageClassStart ),
	kIOUSBMassStorageClassStopCode          = UMC_TRACE ( kIOUSBMassStorageClassStop ),
	kAtUSBAddressCode						= UMC_TRACE ( kAtUSBAddress ),
	kMessagedCalledCode						= UMC_TRACE ( kMessagedCalled ),
	kWillTerminateCalledCode				= UMC_TRACE ( kWillTerminateCalled ),
	kDidTerminateCalledCode					= UMC_TRACE ( kDidTerminateCalled ),
	kCDBLog1Code							= UMC_TRACE ( kCDBLog1 ),
	kCDBLog2Code							= UMC_TRACE ( kCDBLog2 ),
	kClearEndPointStallCode					= UMC_TRACE ( kClearEndPointStall ),
	kGetEndPointStatusCode					= UMC_TRACE ( kGetEndPointStatus ),
	kHandlePowerOnUSBResetCode				= UMC_TRACE ( kHandlePowerOnUSBReset ),
	kUSBDeviceResetWhileTerminatingCode		= UMC_TRACE ( kUSBDeviceResetWhileTerminating ),
	kUSBDeviceResetAfterDisconnectCode		= UMC_TRACE ( kUSBDeviceResetAfterDisconnect ),
	kUSBDeviceResetReturnedCode				= UMC_TRACE ( kUSBDeviceResetReturned ),
	kAbortCurrentSCSITaskCode				= UMC_TRACE ( kAbortCurrentSCSITask ),
	kCompletingCommandWithErrorCode			= UMC_TRACE ( kCompletingCommandWithError ),
	
	// CBI Specific							0x052D0400 - 0x052D07FF
	kCBIProtocolDeviceDetectedCode			= UMC_TRACE ( kCBIProtocolDeviceDetected ),
	kCBICommandAlreadyInProgressCode		= UMC_TRACE ( kCBICommandAlreadyInProgress ),
	kCBISendSCSICommandReturnedCode			= UMC_TRACE ( kCBISendSCSICommandReturned ),
	
	// UFI Specific							0x052D0800 - 0x052D0BFF

	// Bulk-Only Specific					0x052D0C00 - 0x052D0FFF
	kBODeviceDetectedCode					= UMC_TRACE ( kBODeviceDetected ),
	kBOPreferredMaxLUNCode					= UMC_TRACE ( kBOPreferredMaxLUN ),
	kBOGetMaxLUNReturnedCode				= UMC_TRACE ( kBOGetMaxLUNReturned ),
	kBOCommandAlreadyInProgressCode			= UMC_TRACE ( kBOCommandAlreadyInProgress ),
	kBOSendSCSICommandReturnedCode			= UMC_TRACE ( kBOSendSCSICommandReturned ),	
	kBOCBWDescriptionCode					= UMC_TRACE ( kBOCBWDescription ),
	kBOCBWBulkOutWriteResultCode			= UMC_TRACE ( kBOCBWBulkOutWriteResult ),
	kBODoubleCompleteionCode				= UMC_TRACE ( kBODoubleCompleteion ),
	kBOCompletionDuringTerminationCode		= UMC_TRACE ( kBOCompletionDuringTermination ),
	kBOCompletionCode						= UMC_TRACE ( kBOCompletion )
	
};


static const char * kBulkOnlyStateNames[] = {	" ",
												"BulkOnlyCommandSent",
												"BulkOnlyCheckCBWBulkStall",
												"BulkOnlyClearCBWBulkStall",
												"BulkOnlyBulkIOComplete",
												"BulkOnlyCheckBulkStall",
												"BulkOnlyClearBulkStall",
												"BulkOnlyCheckBulkStallPostCSW",
												"BulkOnlyClearBulkStallPostCSW",
												"BulkOnlyStatusReceived",
												"BulkOnlyStatusReceived2ndTime",
												"BulkOnlyResetCompleted",
												"BulkOnlyClearBulkInCompleted",
												"BulkOnlyClearBulkOutCompleted" };


#define kTraceBufferSampleSize			60000
#define kMicrosecondsPerSecond			1000000
#define kMicrosecondsPerMillisecond		1000


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

int					gNumCPUs					= 1;
double				gDivisor					= 0.0;		/* Trace divisor converts to microseconds */
kd_buf *			gTraceBuffer				= NULL;
boolean_t			gTraceEnabled				= FALSE;
boolean_t			gSetRemoveFlag				= TRUE;
boolean_t			gEnableTraceOnly			= FALSE;
const char *		gProgramName				= NULL;
uint32_t			gSavedTraceMask				= 0;
boolean_t			gHideBusyRejectedCommands	= FALSE;

u_int8_t			fullCDB [ 16 ]				= { 0 };
int64_t 			current_usecs				= 0;
int64_t 			prev_usecs					= 0;
int64_t				delta_usecs					= 0;


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
PrintSCSICommand ( void );

static void
PrintTimeStamp ( void );

static void
LoadUSBMassStorageExtension ( void );

static const char * 
StringFromReturnCode ( unsigned int returnCode );


//-----------------------------------------------------------------------------
//	Main
//-----------------------------------------------------------------------------

int
main ( int argc, const char * argv[] )
{
	
	USBSysctlArgs 	args;
	int				error;
	
	gProgramName = argv[0];
	
	if ( reexec_to_match_kernel ( ) != 0 )
	{
		
		fprintf ( stderr, "Could not re-execute to match kernel architecture, errno = %d\n", errno );
		exit ( 1 );
		
	}
	
	if ( geteuid ( ) != 0 )
	{
		
		fprintf ( stderr, "'%s' must be run as root...\n", gProgramName );
		exit ( 1 );
		
	}
	
	// Get program arguments.
	ParseArguments ( argc, argv );
	
	bzero ( &args, sizeof ( args ) );
	
	args.type = kUSBTypeDebug;
	args.operation = kUSBOperationGetFlags;
	
	error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to get old umctrace flags\n" );
	}
	
	args.type = kUSBTypeDebug;
	args.operation = kUSBOperationSetFlags;
	args.debugFlags = 1;
	
	error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
	if ( error != 0 )
	{
		
		LoadUSBMassStorageExtension ( );
		
		error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
		if ( error != 0 )
		{
			fprintf ( stderr, "sysctlbyname failed to set new umctrace flags\n" );
		}
		
	}
	
#if DEBUG
	printf ( "gSavedTraceMask = 0x%08X\n", gSavedTraceMask );
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
	
	printf (	"Usage: %s\n\n\t" 
				"-h help\n\t-r hide rejected SCSI tasks\n\t"
				"-d disable\n", 
				gProgramName );
				
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
		{ "disable",		no_argument,	0, 'd' },
		{ "rejected",		no_argument,	0, 'r' },
		{ "help",			no_argument,	0, 'h' },
		{ 0, 0, 0, 0 }
	};
		
	// If no args specified, enable all logging...
	if ( argc == 1 )
	{
		return;
	}
	
    while ( ( c = getopt_long ( argc, ( char * const * ) argv , "drh?", long_options, NULL  ) ) != -1 )
	{
		
		switch ( c )
		{
			
			case 'd':
				gSavedTraceMask = 0;
				gSetRemoveFlag = FALSE;
				Quit ( "Quit via user-specified trace disable\n" );
				break;
				
			case 'r':
				gHideBusyRejectedCommands = TRUE;
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
	
	kr.type 	= KDBG_SUBCLSTYPE;
	kr.value1 	= DBG_IOKIT;
	kr.value2 	= DBG_IOSAM;
	
	needed = sizeof ( kd_regtype );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	
	if ( sysctl ( mib, 3, &kr, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETREG (subclstype)\n" );
	
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
	
/*
	
	if ( delta_usecs > (100 * kMicrosecondsPerMillisecond )
	{
		printf ( "*** " );
	}
	else
	{
		printf ( "    " );
	}

*/
	
	printf ( "%-8.8s [%lld][%10lld us]", &( ctime ( &currentTime )[11] ), current_usecs, delta_usecs );
	
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
		const char *		errorString;
		
		debugID = gTraceBuffer[index].debugid;
		type	= debugID & ~( DBG_FUNC_START | DBG_FUNC_END );
		
		now = gTraceBuffer[index].timestamp & KDBG_TIMESTAMP_MASK;
		current_usecs = ( int64_t )( now / gDivisor );
		
		if ( ( type >= 0x05278800 ) && ( type <= 0x05278BFC ) && ( type != kCDBLog2Code ) )
		{
			PrintTimeStamp ( );
		}
		
		switch ( type )
		{
		
#pragma mark -
#pragma mark *** Generic UMC Codes ***
#pragma mark -
			
			case kAbortedTaskCode:
			{
				printf ( "[%10p] Task %p Aborted!!!\n", ( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2 );
			}
			break;
			
			case kAbortCurrentSCSITaskCode:
			{
				printf ( "[%10p] Aborted currentTask %p DeviceAttached = %d ConsecutiveResetCount = %d\n",
						( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2,
						( int ) gTraceBuffer[index].arg3, ( int ) gTraceBuffer[index].arg4 );
			}
			break;
				
			case kCompleteSCSICommandCode:
			{
				
				printf ( "[%10p] Task %p Completed with serviceResponse = %d taskStatus = 0x%x\n",
						( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2,
						( int ) gTraceBuffer[index].arg3, ( int ) gTraceBuffer[index].arg4 );
				PrintTimeStamp ( );
				printf ( "[%10p] -------------------------------------------------\n", ( void * ) gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kCompletingCommandWithErrorCode:
			{
				printf ( "[%10p] !!!!! Hark !!!!! Completing command with an ERROR status!\n", ( void * ) gTraceBuffer[index].arg1 );
			}
			break;
			
			case kLUNConfigurationCompleteCode:
			{
				printf ( "[%10p] MaxLUN = %u\n", ( void * ) gTraceBuffer[index].arg1, ( unsigned int ) gTraceBuffer[index].arg2 );
			}
			break;
			
			case kNewCommandWhileTerminatingCode:
			{
				printf ( "[%10p] Task = %p received while terminating!!!\n", ( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2 );
			}
			break;
			
			case kIOUMCStorageCharacDictFoundCode:
			{
				printf ( "[%10p] This device has a USB Characteristics Dictionary\n", ( void * ) gTraceBuffer[index].arg1 );
			}
			break;
			
			case kNoProtocolForDeviceCode:
			{
				printf ( "[%10p] !!! NO USB TRANSPORT PROTOCOL FOR THIS DEVICE !!!\n", ( void * ) gTraceBuffer[index].arg1 );
			}
			break;
			
			case kIOUSBMassStorageClassStartCode:
			{
				printf ( "[%10p] Starting up!\n", ( void * ) gTraceBuffer[index].arg1 );
			}
			break;
			
			case kIOUSBMassStorageClassStopCode:
			{
				printf ( "[%10p] Stopping!\n", ( void * ) gTraceBuffer[index].arg1 );
			}
			break;
			
			case kAtUSBAddressCode:
			{
				printf ( "[%10p] @ USB Address: %u\n", ( void * ) gTraceBuffer[index].arg1, ( unsigned int ) gTraceBuffer[index].arg2 );
			}
			break;
			
			case kMessagedCalledCode:
			{
				printf ( "[%10p] Message : %x received\n", ( void * ) gTraceBuffer[index].arg1, ( unsigned int ) gTraceBuffer[index].arg2 );
				PrintTimeStamp ( );
				printf ( "[%10p] -------------------------------------------------\n", ( void * ) gTraceBuffer[index].arg1 );
			}
			break;
			
			case kWillTerminateCalledCode:
			{
				printf ( "[%10p] willTerminate called, CurrentInterface=%p, isInactive=%u\n", 
					( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2, ( unsigned int ) gTraceBuffer[index].arg3 );
			}
			break;
			
			case kDidTerminateCalledCode:
			{
				printf ( "[%10p] didTerminate called, fTerminationDeferred=%u\n", 
						( void * ) gTraceBuffer[index].arg1, ( unsigned int ) gTraceBuffer[index].arg2 );
			}
			break;
			
			case kCDBLog1Code:
			{

				UInt8 *			cdbData;
				unsigned int	i;
				
				printf ( "[%10p] Request %p\n", ( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2 );
				PrintTimeStamp ( );
				printf ( "[%10p] ", ( void * ) gTraceBuffer[index].arg1 );
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg3;
				
				for ( i = 0; i < 4; i++ )
				{
					fullCDB [i] = cdbData[i];
					printf ( "0x%02X : ", cdbData[i] );
				}
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg4;
				
				for ( i = 0; i < 4; i++ )
				{
					fullCDB [i+4] = cdbData[i];
					printf ( "0x%02X : ", cdbData[i] );
				}
				
			}
			break;		
			
			case kCDBLog2Code:
			{

				UInt8 *			cdbData;
				unsigned int 	i;
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg3;
				
				for ( i = 0; i < 4; i++ )
				{
					fullCDB [i+8] = cdbData[i];
					printf ( "0x%02X : ", cdbData[i] );
				}
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg4;
				
				for ( i = 0; i < 3; i++ )
				{
					fullCDB [i+12] = cdbData[i];
					printf ( "0x%02X : ", cdbData[i] );
				}
				
				fullCDB [i+12] = cdbData[i];
				printf ( "0x%02X\n", cdbData[i] );
				
				PrintTimeStamp ( );
				printf ( "[%10p] ", ( void * ) gTraceBuffer[index].arg1 );
				PrintSCSICommand ( );
				
			}
			break;	
			
			case kClearEndPointStallCode:
			{
				
				errorString = StringFromReturnCode ( gTraceBuffer[index].arg2 );
				printf ( "[%10p] ClearFeatureEndpointStall status=%s (0x%x), endpoint=%u\n", 
						( void * ) gTraceBuffer[index].arg1, errorString, ( unsigned int ) gTraceBuffer[index].arg2, ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;
			
			case kGetEndPointStatusCode:
			{
				
				errorString = StringFromReturnCode ( gTraceBuffer[index].arg2 );
				printf ( "[%10p] GetEndpointStatus status=%s (0x%x), endpoint=%u\n", 
						( void * ) gTraceBuffer[index].arg1, errorString, ( unsigned int ) gTraceBuffer[index].arg2, ( unsigned int ) gTraceBuffer[index].arg3 );		
				
			}
			break;
			
			case kHandlePowerOnUSBResetCode:
			{
				
				printf ( "[%10p] USB Device Reset on WAKE from SLEEP\n", ( void * ) gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kUSBDeviceResetWhileTerminatingCode:
			{
				
				printf ( "[%10p] Termination started before device reset could be initiated! fTerminating=%u, isInactive=%u\n", 
                            ( void * ) gTraceBuffer[index].arg1, ( unsigned int ) gTraceBuffer[index].arg2, ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;
			
			case kUSBDeviceResetAfterDisconnectCode:
			{
				
				printf ( "[%10p] Device reset was attempted after the device had been disconnected\n", ( void * ) gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kUSBDeviceResetReturnedCode:
			{
				
				printf ( "[%10p] DeviceReset returned: 0x%08x\n", ( void * ) gTraceBuffer[index].arg1, ( unsigned int ) gTraceBuffer[index].arg2 );
				
			}
			break;
			
#pragma mark -
#pragma mark *** Control Bulk Interrupt ( CBI ) Codess ***
#pragma mark -
			
			case kCBIProtocolDeviceDetectedCode:
			{
				
				printf ( "[%10p] CBI transport protocol device\n", ( void * ) gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kCBICommandAlreadyInProgressCode:
			{
			
				if ( gHideBusyRejectedCommands == FALSE )
				{
					
					printf ( "[%10p] CBI - Unable to accept task %p, still working on previous command\n", 
								( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2 );
					
				}
				
			}
			break;
			
			case kCBISendSCSICommandReturnedCode:
			{
				
				errorString = StringFromReturnCode ( gTraceBuffer[index].arg3 );
				printf ( "[%10p] CBI - SCSI Task %p was sent with status %s (0x%x)\n", 
							( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2, errorString, ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;
			
#pragma mark -
#pragma mark *** Bulk-Only Protocol Codes ***
#pragma mark -
			
			case kBODeviceDetectedCode:
			{
				
				printf ( "[%10p] BULK-ONLY transport protocol device\n", ( void * ) gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kBOCommandAlreadyInProgressCode:
			{
				
				if ( gHideBusyRejectedCommands == FALSE )
				{
					
					printf ( "[%10p] B0 - Unable to accept task %p, still working on previous request\n", 
								( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2 );
					
				}
				
			}
			break;
			
			case kBOSendSCSICommandReturnedCode:
			{
				
				errorString = StringFromReturnCode ( gTraceBuffer[index].arg3 );
				printf ( "[%10p] BO - SCSI Task %p was sent with status %s (0x%x)\n", 
								( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2, errorString, ( unsigned int ) gTraceBuffer[index].arg3 );				
				
			}
			break;
			
			case kBOPreferredMaxLUNCode:
			{
				
				printf ( "[%10p] BO - Preferred MaxLUN: %d\n", 
							( void * ) gTraceBuffer[index].arg1, (int) gTraceBuffer[index].arg2 );
				
			}
			break;
			
			case kBOGetMaxLUNReturnedCode:
			{
				
				errorString = StringFromReturnCode ( gTraceBuffer[index].arg2 );
				printf ( "[%10p] BO - GetMaxLUN returned: %s (0x%x), triedReset=%u, MaxLun: %d\n", 
								( void * ) gTraceBuffer[index].arg1, errorString, ( unsigned int ) gTraceBuffer[index].arg2, ( unsigned int ) gTraceBuffer[index].arg4, ( unsigned int ) gTraceBuffer[index].arg3 );
				
			}
			break;
			
			case kBOCBWDescriptionCode:
			{
				
				printf ( "[%10p] BO - Request %p, LUN: %u, CBW Tag: %u (0x%x)\n", 
							( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg2, ( unsigned int ) gTraceBuffer[index].arg3, ( unsigned int ) gTraceBuffer[index].arg4, ( unsigned int ) gTraceBuffer[index].arg4 );
				
			}
			break;
			
			case kBOCBWBulkOutWriteResultCode:
			{
				
				errorString = StringFromReturnCode ( gTraceBuffer[index].arg2 );
				printf ( "[%10p] BO - Request %p, LUN: %u, Bulk-Out Write Status: %s (0x%x)\n", 
							( void * ) gTraceBuffer[index].arg1, ( void * ) gTraceBuffer[index].arg4, ( unsigned int ) gTraceBuffer[index].arg3, errorString, ( unsigned int ) gTraceBuffer[index].arg2 );
				
			}
			break;
		
			case kBODoubleCompleteionCode:
			{
				
				printf ( "[%10p] BO - DOUBLE Completion\n", ( void * ) gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kBOCompletionDuringTerminationCode:
			{
				
				printf ( "[%10p] BO - Completion during termination\n", ( void * ) gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kBOCompletionCode:
			{
				
				errorString = StringFromReturnCode ( gTraceBuffer[index].arg2 );
				printf ( "[%10p] BO - Completion, State: %s, Status: %s (0x%x), for Request: %p\n", 
								( void * ) gTraceBuffer[index].arg1, kBulkOnlyStateNames [ (int) gTraceBuffer[index].arg3 ], 
								errorString, ( unsigned int ) gTraceBuffer[index].arg2, ( void * ) gTraceBuffer[index].arg4 );
				
			}
			break;
			
			default:
			{
				
				if ( ( type >= 0x05278800 ) && ( type <= 0x05278BFC ) )
				{
					printf ( "[%10p] ??? - UNEXPECTED USB TRACE POINT - %p\n", ( void * ) gTraceBuffer[index].arg1, ( void * ) type );
				}
				
			}
			break;
			
		}
		
	}
	
	fflush ( 0 );
	
}


//-----------------------------------------------------------------------------
//	Quit
//-----------------------------------------------------------------------------

static void
Quit ( const char * s )
{
	
	USBSysctlArgs	args;
	int				error;
	
	if ( gTraceEnabled == TRUE )
		EnableTraceBuffer ( 0 );
	
	if ( gSetRemoveFlag == TRUE )
		RemoveTraceBuffer ( );
	
	args.type = kUSBTypeDebug;
	args.debugFlags = 0;
	
	error = sysctlbyname ( USBMASS_SYSCTL, NULL, NULL, &args, sizeof ( args ) );
	if ( error != 0 )
	{
		fprintf ( stderr, "sysctlbyname failed to set old UMC trace flags back\n" );
	}
	
	fprintf ( stderr, "%s: ", gProgramName );
	if ( s != NULL )
	{
		fprintf ( stderr, "%s", s );
	}
	
	exit ( 1 );
	
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


//-----------------------------------------------------------------------------
//	LoadUSBMassStorageExtension
//-----------------------------------------------------------------------------

static void
LoadUSBMassStorageExtension ( void )
{

	posix_spawn_file_actions_t	fileActions;
	char * const				argv[]	= { ( char * ) "/sbin/kextload", ( char * ) "/System/Library/Extensions/IOUSBMassStorageClass.kext", NULL };
	char * const				env[]	= { NULL };
	pid_t						child	= 0;
	union wait 					status;
	
	posix_spawn_file_actions_init ( &fileActions );
	posix_spawn_file_actions_addclose ( &fileActions, STDOUT_FILENO );
	posix_spawn_file_actions_addclose ( &fileActions, STDERR_FILENO );
	
	posix_spawn ( &child, "/sbin/kextload", &fileActions, NULL, argv, env );
	
	if ( !( ( wait4 ( child, ( int * ) &status, 0, NULL ) == child ) && ( WIFEXITED ( status ) ) ) )
	{
		printf ( "Error loading USB Mass Storage extension\n" );
	}	
	
	posix_spawn_file_actions_destroy ( &fileActions );

}


//-----------------------------------------------------------------------------
//	StringFromReturnCode
//-----------------------------------------------------------------------------

static const char * 
StringFromReturnCode ( unsigned int returnCode )
{
	
	const char *	string = "UNKNOWN";
	unsigned int	i;
	
	static ReturnCodeSpec	sReturnCodeSpecs[] =
	{
		
		//	USB Return codes
		{ kIOUSBUnknownPipeErr,								"kIOUSBUnknownPipeErr" },
		{ kIOUSBTooManyPipesErr,							"kIOUSBTooManyPipesErr" },
		{ kIOUSBNoAsyncPortErr,								"kIOUSBNoAsyncPortErr" },
		{ kIOUSBNotEnoughPipesErr,							"kIOUSBNotEnoughPipesErr" },
		{ kIOUSBNotEnoughPowerErr,							"kIOUSBNotEnoughPowerErr" },
		{ kIOUSBEndpointNotFound,							"kIOUSBEndpointNotFound" },
		{ kIOUSBConfigNotFound,								"kIOUSBConfigNotFound" },
		{ kIOUSBTransactionTimeout,							"kIOUSBTransactionTimeout" },
		{ kIOUSBTransactionReturned,						"kIOUSBTransactionReturned" },
		{ kIOUSBPipeStalled,								"kIOUSBPipeStalled" },
		{ kIOUSBInterfaceNotFound,							"kIOUSBInterfaceNotFound" },
		{ kIOUSBLowLatencyBufferNotPreviouslyAllocated,		"kIOUSBLowLatencyBufferNotPreviouslyAllocated" },
		{ kIOUSBLowLatencyFrameListNotPreviouslyAllocated,	"kIOUSBLowLatencyFrameListNotPreviouslyAllocated" },
		{ kIOUSBHighSpeedSplitError,						"kIOUSBHighSpeedSplitError" },
		{ kIOUSBSyncRequestOnWLThread,						"kIOUSBSyncRequestOnWLThread" },
		{ kIOUSBDeviceNotHighSpeed,							"kIOUSBDeviceNotHighSpeed" },
		{ kIOUSBLinkErr,									"kIOUSBLinkErr" },
		{ kIOUSBNotSent2Err,								"kIOUSBNotSent2Err" },
		{ kIOUSBNotSent1Err,								"kIOUSBNotSent1Err" },
		{ kIOUSBBufferUnderrunErr,							"kIOUSBBufferUnderrunErr" },
		{ kIOUSBBufferOverrunErr,							"kIOUSBBufferOverrunErr" },
		{ kIOUSBReserved2Err,								"kIOUSBReserved2Err" },
		{ kIOUSBReserved1Err,								"kIOUSBReserved1Err" },
		{ kIOUSBWrongPIDErr,								"kIOUSBWrongPIDErr" },
		{ kIOUSBPIDCheckErr,								"kIOUSBPIDCheckErr" },
		{ kIOUSBDataToggleErr,								"kIOUSBDataToggleErr" },
		{ kIOUSBBitstufErr,									"kIOUSBBitstufErr" },
		{ kIOUSBCRCErr,										"kIOUSBCRCErr" },
		
		//	IOReturn codes
		{ kIOReturnSuccess,									"kIOReturnSuccess" },
		{ kIOReturnError,									"kIOReturnError" },
		{ kIOReturnNoMemory,								"kIOReturnNoMemory" },
		{ kIOReturnNoResources,								"kIOReturnNoResources" },
		{ kIOReturnIPCError,								"kIOReturnIPCError" },
		{ kIOReturnNoDevice,								"kIOReturnNoDevice" },
		{ kIOReturnNotPrivileged,							"kIOReturnNotPrivileged" },
		{ kIOReturnBadArgument,								"kIOReturnBadArgument" },
		{ kIOReturnLockedRead,								"kIOReturnLockedRead" },
		{ kIOReturnLockedWrite,								"kIOReturnLockedWrite" },
		{ kIOReturnExclusiveAccess,							"kIOReturnExclusiveAccess" },
		{ kIOReturnBadMessageID,							"kIOReturnBadMessageID" },
		{ kIOReturnUnsupported,								"kIOReturnUnsupported" },
		{ kIOReturnVMError,									"kIOReturnVMError" },
		{ kIOReturnInternalError,							"kIOReturnInternalError" },
		{ kIOReturnIOError,									"kIOReturnIOError" },
		{ kIOReturnCannotLock,								"kIOReturnCannotLock" },
		{ kIOReturnNotOpen,									"kIOReturnNotOpen" },
		{ kIOReturnNotReadable,								"kIOReturnNotReadable" },
		{ kIOReturnNotWritable,								"kIOReturnNotWritable" },
		{ kIOReturnNotAligned,								"kIOReturnNotAligned" },
		{ kIOReturnBadMedia,								"kIOReturnBadMedia" },
		{ kIOReturnStillOpen,								"kIOReturnStillOpen" },
		{ kIOReturnRLDError,								"kIOReturnRLDError" },
		{ kIOReturnDMAError,								"kIOReturnDMAError" },
		{ kIOReturnBusy,									"kIOReturnBusy" },
		{ kIOReturnTimeout,									"kIOReturnTimeout" },
		{ kIOReturnOffline,									"kIOReturnOffline" },
		{ kIOReturnNotReady,								"kIOReturnNotReady" },
		{ kIOReturnNotAttached,								"kIOReturnNotAttached" },
		{ kIOReturnNoChannels,								"kIOReturnNoChannels" },
		{ kIOReturnNoSpace,									"kIOReturnNoSpace" },
		{ kIOReturnPortExists,								"kIOReturnPortExists" },
		{ kIOReturnCannotWire,								"kIOReturnCannotWire" },
		{ kIOReturnNoInterrupt,								"kIOReturnNoInterrupt" },
		{ kIOReturnNoFrames,								"kIOReturnNoFrames" },
		{ kIOReturnMessageTooLarge,							"kIOReturnMessageTooLarge" },
		{ kIOReturnNotPermitted,							"kIOReturnNotPermitted" },
		{ kIOReturnNoPower,									"kIOReturnNoPower" },
		{ kIOReturnNoMedia,									"kIOReturnNoMedia" },
		{ kIOReturnUnformattedMedia,						"kIOReturnUnformattedMedia" },
		{ kIOReturnUnsupportedMode,							"kIOReturnUnsupportedMode" },
		{ kIOReturnUnderrun,								"kIOReturnUnderrun" },
		{ kIOReturnOverrun,									"kIOReturnOverrun" },
		{ kIOReturnDeviceError,								"kIOReturnDeviceError" },
		{ kIOReturnNoCompletion,							"kIOReturnNoCompletion" },
		{ kIOReturnAborted,									"kIOReturnAborted" },
		{ kIOReturnNoBandwidth,								"kIOReturnNoBandwidth" },
		{ kIOReturnNotResponding,							"kIOReturnNotResponding" },
		{ kIOReturnIsoTooOld,								"kIOReturnIsoTooOld" },
		{ kIOReturnIsoTooNew,								"kIOReturnIsoTooNew" },
		{ kIOReturnNotFound,								"kIOReturnNotFound" },
		{ kIOReturnInvalid,									"kIOReturnInvalid" }
	};
	
	for ( i = 0; i < ( sizeof ( sReturnCodeSpecs ) / sizeof ( sReturnCodeSpecs[0] ) ); i++ )
	{
		
		if ( returnCode == sReturnCodeSpecs[i].returnCode )
		{
			
			string = sReturnCodeSpecs[i].string;
			break;
			
		}
		
	}
	
	return string;
	
}

//-----------------------------------------------------------------------------
//	EOF
//-----------------------------------------------------------------------------
