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

/*
g++ -W -Wall -I/System/Library/Frameworks/System.framework/PrivateHeaders -I/System/Library/Frameworks/Kernel.framework/PrivateHeaders -DPRIVATE -D__APPLE_PRIVATE -O -arch ppc -arch i386 -o UMCLogger UMCLogger.cpp
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
#include <IOKit/usb/IOUSBMassStorageClassTimestamps.h>
#include <IOKit/usb/USB.h>

#define DEBUG 			0


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
	kUSBDeviceResetWhileTerminatingCode		= ( UMC_TRACE ( kUSBDeviceResetWhileTerminating ) | DBG_FUNC_START ),
	kUSBDeviceResetWhileTerminating_2Code	= ( UMC_TRACE ( kUSBDeviceResetWhileTerminating ) | DBG_FUNC_END ),
	kUSBDeviceResetAfterDisconnectCode		= UMC_TRACE ( kUSBDeviceResetAfterDisconnect ),
	kUSBDeviceResetReturnedCode				= UMC_TRACE ( kUSBDeviceResetReturned ),
	kAbortCurrentSCSITaskCode				= UMC_TRACE ( kAbortCurrentSCSITask ),
	
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

int					gBiasSeconds				= 0;
double				gDivisor					= 0.0;		/* Trace divisor converts to microseconds */
kd_buf *			gTraceBuffer				= NULL;
boolean_t			gTraceEnabled				= FALSE;
boolean_t			gSetRemoveFlag				= TRUE;
boolean_t			gEnableTraceOnly			= FALSE;
const char *		gProgramName				= NULL;
uint32_t			gSavedTraceMask				= 0;
boolean_t			gHideBusyRejectedCommands	= FALSE;


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
LoadUSBMassStorageExtension ( void );

static boolean_t 
StringFromReturnCode ( unsigned returnCode, char * outString );

static boolean_t
StringFromIOReturn ( unsigned ioreturnCode, char * outString );

static boolean_t
StringFromUSBReturn ( unsigned ioReturnCode, char * outString );


//-----------------------------------------------------------------------------
//	Main
//-----------------------------------------------------------------------------

int
main ( int argc, const char * argv[] )
{
	
	USBSysctlArgs 	args;
	int				error;
	
	gProgramName = argv[0];
	
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
		
		LoadUSBMassStorageExtension();
		
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
		char				errorString_1[64];
		
		
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
		
#pragma mark -
#pragma mark *** Generic UMC Codes ***
#pragma mark -
			
			case kAbortedTaskCode:
			{
				printf ( "[%p] Task %p Aborted!!!\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
			case kCompleteSCSICommandCode:
			{
				
				if ( StringFromReturnCode ( gTraceBuffer[index].arg3, errorString_1 ) )
				{
					printf ( "[%p] Task %p Completed with status = %s (0x%x)\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, errorString_1, gTraceBuffer[index].arg3  );				
				}
				else
				{
					printf ( "[%p] Task %p Completed with status = %x\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3  );
				}
				
				printf ( "[%p] -------------------------------------------------\n", gTraceBuffer[index].arg1 );
				
			}
			break;
			
			case kLUNConfigurationCompleteCode:
			{
				printf ( "[%p] MaxLUN = %u\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
			case kNewCommandWhileTerminatingCode:
			{
				printf ( "[%p] Task = %p received while terminating!!!\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
			case kIOUMCStorageCharacDictFoundCode:
			{
				printf ( "[%p] This device has a USB Characteristics Dictionary\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kNoProtocolForDeviceCode:
			{
				printf ( "[%p] !!! NO USB TRANSPORT PROTOCOL FOR THIS DEVICE !!!\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kIOUSBMassStorageClassStartCode:
			{
				printf ( "[%p] Starting up!\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kIOUSBMassStorageClassStopCode:
			{
				printf ( "[%p] Stopping!\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kAtUSBAddressCode:
			{
				printf ( "[%p] @ USB Address: %u\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
			case kMessagedCalledCode:
			{
				printf ( "[%p] Message : %x recieved\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
			case kWillTerminateCalledCode:
			{
				printf ( "[%p] willTerminate called, CurrentInterface=%p, isInactive=%u\n", 
					gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3);
			}
			break;
			
			case kDidTerminateCalledCode:
			{
				printf ( "[%p] didTerminate called, CurrentInterface=%p, isInactive=%u, fResetInProgress=%u\n", 
					gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3, gTraceBuffer[index].arg4);
			}
			break;
			
			case kCDBLog1Code:
			{

				UInt8 * cdbData;
				unsigned i;
				
				printf ( "[%p] Request %p\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
				printf ( "[%p] ", gTraceBuffer[index].arg1 );
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg3;
				
				for ( i = 0; i < 4; i++ ) printf ( "%x : ", cdbData[i] );
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg4;
				
				for ( i = 0; i < 4; i++ ) printf ( "%x : ", cdbData[i] );

			}
			break;		
			
			case kCDBLog2Code:
			{

				UInt8 * cdbData;
				unsigned i;
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg3;
				
				for ( i = 0; i < 4; i++ ) printf ( "%x : ", cdbData[i] );
				
				cdbData = ( UInt8 * ) &gTraceBuffer[index].arg4;
				
				for ( i = 0; i < 3; i++ ) printf ( "%x : ", cdbData[i] );
			
				printf ( "%x\n", cdbData[i] );

			}
			break;	
			
			case kClearEndPointStallCode:
			{
			
				if ( StringFromReturnCode ( gTraceBuffer[index].arg2, errorString_1 ) )
				{
					printf ( "[%p] ClearFeatureEndpointStall status=%s (0x%x), endpoint=%u\n", 
						gTraceBuffer[index].arg1, errorString_1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );
				}
				else
				{
					printf ( "[%p] ClearFeatureEndpointStall status=%x, endpoint=%u\n", 
						gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );
				}
				
			}
			break;
			
			case kGetEndPointStatusCode:
			{
			
				if ( StringFromReturnCode ( gTraceBuffer[index].arg2, errorString_1 ) )
				{
					printf ( "[%p] ClearFeatureEndpointStall status=%s (%x), endpoint=%u\n", 
						gTraceBuffer[index].arg1, errorString_1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );		
				}
				else
				{
					printf ( "[%p] ClearFeatureEndpointStall status=%x, endpoint=%u\n", 
						gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );
				}
				
			}
			break;
			
			case kHandlePowerOnUSBResetCode:
			{
				printf ( "[%p] USB Device Reset on WAKE from SLEEP\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kUSBDeviceResetWhileTerminatingCode:
			{
				printf ( "%p Termination started before device reset could be initiated! fTerminating=%u, isInactive=%u\n", 
                            gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );
			}
			break;
			
			case kUSBDeviceResetWhileTerminating_2Code:
			{
				printf ( "[%p] Termination occurred while we were reseting the device! fTerminating=%u, isInactive=%u\n", 
                            gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );
			}
			break;
			
			case kUSBDeviceResetAfterDisconnectCode:
			{
				printf ( "[%p] Device reset was attempted after the device had been disconnected\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kUSBDeviceResetReturnedCode:
			{
				printf ( "[%p] DeviceReset returned: %u\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
			case kAbortCurrentSCSITaskCode:
			{
				printf ( "[%p] sAbortCurrentSCSITask device attached: %u\n", gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
#pragma mark -
#pragma mark *** Control Bulk Interrupt ( CBI ) Codess ***
#pragma mark -
			
			case kCBIProtocolDeviceDetectedCode:
			{
				printf ( "[%p] CBI transport protocol device\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kCBICommandAlreadyInProgressCode:
			{
			
				if ( gHideBusyRejectedCommands == FALSE )
				{
					printf ( "[%p] CBI - Unable to accept task %p, still working on previous command\n", 
								gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
				}
				
			}
			break;
			
			case kCBISendSCSICommandReturnedCode:
			{
			
				if ( StringFromReturnCode ( gTraceBuffer[index].arg3, errorString_1 ) )
				{
					printf ( "[%p] CBI - SCSI Task %p was sent with status %s (0x%x)\n", 
							gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, errorString_1, 
							gTraceBuffer[index].arg3 );
				}
				else
				{
					printf ( "[%p] CBI - SCSI Task %p was sent with status %x\n", 
							gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );
				}
				
			}
			break;
			
#pragma mark -
#pragma mark *** Bulk-Only Protocol Codes ***
#pragma mark -
			
			case kBODeviceDetectedCode:
			{
				printf ( "[%p] BULK-ONLY transport protocol device\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kBOCommandAlreadyInProgressCode:
			{
			
				if ( gHideBusyRejectedCommands == FALSE )
				{

					printf ( "[%p] B0 Unable to accept task %p, still working on previous request\n", 
								gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
                                
				}
				
			}
			break;
			
			case kBOSendSCSICommandReturnedCode:
			{
				
				if ( StringFromIOReturn ( gTraceBuffer[index].arg3, errorString_1 ) )
				{
					printf ( "[%p] BO - SCSI Task %p was sent with status %s (0x%x)\n", 
								gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, errorString_1, gTraceBuffer[index].arg3 );				
				}
				else
				{
					printf ( "[%p] BO - SCSI Task %p was sent with status %x\n", 
								gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3 );
				}
				
			}
			break;
			
			case kBOPreferredMaxLUNCode:
			{
				printf ( "[%p] BO - Preferred MaxLUN: %d\n", 
							gTraceBuffer[index].arg1, gTraceBuffer[index].arg2 );
			}
			break;
			
			case kBOGetMaxLUNReturnedCode:
			{
			
				if ( StringFromReturnCode ( gTraceBuffer[index].arg2, errorString_1 ) )
				{
					printf ( "[%p] BO - GetMaxLUN returned: %s (0x%x), triedReset=%u, MaxLun: %d\n", 
								gTraceBuffer[index].arg1, errorString_1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg4, gTraceBuffer[index].arg3 );
				}
				else
				{
					printf ( "[%p] BO - GetMaxLUN returned: %x, triedReset=%u, MaxLun: %d\n", 
							gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg4, gTraceBuffer[index].arg3 );
				}
				
			}
			break;
			
			case kBOCBWDescriptionCode:
			{
				printf ( "[%p] BO - Request %p, LUN: %u, CBW Tag: %u (0x%x)\n", 
							gTraceBuffer[index].arg1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg3, gTraceBuffer[index].arg4, gTraceBuffer[index].arg4 );
			}
			break;
			
			case kBOCBWBulkOutWriteResultCode:
			{
			
				if ( StringFromReturnCode ( gTraceBuffer[index].arg2, errorString_1 ) )
				{
					printf ( "[%p] BO - Request %p, LUN: %u, Bulk-Out Write Status: %s (0x%x)\n", 
								gTraceBuffer[index].arg1, gTraceBuffer[index].arg4, gTraceBuffer[index].arg3, errorString_1, gTraceBuffer[index].arg2 );
				}
				else
				{
					printf ( "[%p] BO - Request %p, LUN: %u, Bulk-Out Write Status: %x\n", 
								gTraceBuffer[index].arg1, gTraceBuffer[index].arg4, gTraceBuffer[index].arg3, gTraceBuffer[index].arg2 );
				}
				
			}
			break;
		
			case kBODoubleCompleteionCode:
			{
				printf ( "[%p] BO - DOUBLE Comletion\n", gTraceBuffer[index].arg1 );
				printf ( "[%p] BO - DOUBLE Comletion\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kBOCompletionDuringTerminationCode:
			{
				printf ( "[%p] BO - Completion during termination\n", gTraceBuffer[index].arg1 );
			}
			break;
			
			case kBOCompletionCode:
			{
			
				if ( StringFromReturnCode ( gTraceBuffer[index].arg2, errorString_1 ) )
				{
					printf ( "[%p] BO - Completion, State: %s, Status: %s (0x%x), for Request: %p\n", 
								gTraceBuffer[index].arg1, kBulkOnlyStateNames [ gTraceBuffer[index].arg3 ], 
								errorString_1, gTraceBuffer[index].arg2, gTraceBuffer[index].arg4 );
				}
				else
				{
					printf ( "[%p] BO - Completion, State: %s, Status: %x, for Request: %p\n", 
								gTraceBuffer[index].arg1, kBulkOnlyStateNames [ gTraceBuffer[index].arg3 ], 
								gTraceBuffer[index].arg2, gTraceBuffer[index].arg4 );
				}
				
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
	char * const	argv[]	= { "/sbin/kextload", "/System/Library/Extensions/IOUSBMassStorageClass.kext", NULL };
	char * const	env[]	= { NULL };
	pid_t			child	= 0;
	union wait 		status;
	
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

static boolean_t 
StringFromReturnCode ( unsigned returnCode, char * outString )
{

	boolean_t returnValue = FALSE;
	
	
	returnValue = StringFromIOReturn ( returnCode, outString );
	if ( returnValue ) goto Exit;
	
	returnValue = StringFromUSBReturn ( returnCode, outString );
	if ( returnValue ) goto Exit;
	
	
Exit:

	return returnValue;
	
}

//-----------------------------------------------------------------------------
//	StringFromIOReturn
//-----------------------------------------------------------------------------

static boolean_t
StringFromIOReturn ( unsigned ioReturnCode, char * outString )
{

	boolean_t returnValue = FALSE;
	
	switch ( ioReturnCode )
	{
	
		// #define kIOReturnSuccess         KERN_SUCCESS            // OK
		case kIOReturnSuccess:
		{	
			strncpy ( outString, "kIOReturnSuccess", sizeof ( "kIOReturnSuccess" ) );
		}
		break;
		
		// #define kIOReturnError           iokit_common_err(0x2bc) // general error 
		case kIOReturnError:
		{	
			strncpy ( outString, "kIOReturnError", sizeof ( "kIOReturnError" ) );
		}
		break;
		
		// #define kIOReturnNoMemory        iokit_common_err(0x2bd) // can't allocate memory 
		case kIOReturnNoMemory:
		{	
			strncpy ( outString, "kIOReturnNoMemory", sizeof ( "kIOReturnNoMemory" ) );
		}
		break;
		
		// #define kIOReturnNoResources     iokit_common_err(0x2be) // resource shortage 
		case kIOReturnNoResources:
		{	
			strncpy ( outString, "kIOReturnNoResources", sizeof ( "kIOReturnNoResources" ) );
		}
		break;
		
		// #define kIOReturnIPCError        iokit_common_err(0x2bf) // error during IPC 
		case kIOReturnIPCError:
		{	
			strncpy ( outString, "kIOReturnIPCError", sizeof ( "kIOReturnIPCError" ) );
		}
		break;
		
		// #define kIOReturnNoDevice        iokit_common_err(0x2c0) // no such device 
		case kIOReturnNoDevice:
		{	
			strncpy ( outString, "kIOReturnNoDevice", sizeof ( "kIOReturnNoDevice" ) );
		}
		break;
		
		// #define kIOReturnNotPrivileged   iokit_common_err(0x2c1) // privilege violation 
		case kIOReturnNotPrivileged:
		{	
			strncpy ( outString, "kIOReturnNotPrivileged", sizeof ( "kIOReturnNotPrivileged" ) );
		}
		break;
		
		// #define kIOReturnBadArgument     iokit_common_err(0x2c2) // invalid argument 
		case kIOReturnBadArgument:
		{	
			strncpy ( outString, "kIOReturnBadArgument", sizeof ( "kIOReturnBadArgument" ) );
		}
		break;

		// #define kIOReturnLockedRead      iokit_common_err(0x2c3) // device read locked 
		case kIOReturnLockedRead:
		{	
			strncpy ( outString, "kIOReturnLockedRead", sizeof ( "kIOReturnLockedRead" ) );
		}
		break;
		
		// #define kIOReturnLockedWrite     iokit_common_err(0x2c4) // device write locked 
		case kIOReturnLockedWrite:
		{	
			strncpy ( outString, "kIOReturnLockedWrite", sizeof ( "kIOReturnLockedWrite" ) );
		}
		break;
		
		// #define kIOReturnExclusiveAccess iokit_common_err(0x2c5) // exclusive access and device already open 
		case kIOReturnExclusiveAccess:
		{	
			strncpy ( outString, "kIOReturnExclusiveAccess", sizeof ( "kIOReturnExclusiveAccess" ) );
		}
		break;

		// #define kIOReturnBadMessageID    iokit_common_err(0x2c6) // sent/received messages had different msg_id
		case kIOReturnBadMessageID:
		{	
			strncpy ( outString, "kIOReturnExclusiveAccess", sizeof ( "kIOReturnExclusiveAccess" ) );
		}
		break;
		
		// #define kIOReturnUnsupported     iokit_common_err(0x2c7) // unsupported function 
		case kIOReturnUnsupported:
		{	
			strncpy ( outString, "kIOReturnUnsupported", sizeof ( "kIOReturnUnsupported" ) );
		}
		break;
		
		// #define kIOReturnVMError         iokit_common_err(0x2c8) // misc. VM failure 
		case kIOReturnVMError:
		{	
			strncpy ( outString, "kIOReturnVMError", sizeof ( "kIOReturnVMError" ) );\
		}
		break;

		// #define kIOReturnInternalError   iokit_common_err(0x2c9) // internal error 
		case kIOReturnInternalError:
		{	
			strncpy ( outString, "kIOReturnInternalError", sizeof ( "kIOReturnInternalError" ) );
		}
		break;

		// #define kIOReturnIOError         iokit_common_err(0x2ca) // General I/O error 
		case kIOReturnIOError:
		{	
			strncpy ( outString, "kIOReturnIOError", sizeof ( "kIOReturnIOError" ) );
		}
		break;
		
		// #define kIOReturnCannotLock      iokit_common_err(0x2cc) // can't acquire lock
		case kIOReturnCannotLock:
		{	
			strncpy ( outString, "kIOReturnCannotLock", sizeof ( "kIOReturnCannotLock" ) );
		}
		break;
		
		// #define kIOReturnNotOpen         iokit_common_err(0x2cd) // device not open 
		case kIOReturnNotOpen:
		{	
			strncpy ( outString, "kIOReturnNotOpen", sizeof ( "kIOReturnNotOpen" ) );
		}
		break;
		
		// #define kIOReturnNotReadable     iokit_common_err(0x2ce) // read not supported 
		case kIOReturnNotReadable:
		{	
			strncpy ( outString, "kIOReturnNotReadable", sizeof ( "kIOReturnNotReadable" ) );
		}
		break;
		
		// #define kIOReturnNotWritable     iokit_common_err(0x2cf) // write not supported 
		case kIOReturnNotWritable:
		{	
			strncpy ( outString, "kIOReturnNotWritable", sizeof ( "kIOReturnNotWritable" ) );
		}
		break;
		
		// #define kIOReturnNotAligned      iokit_common_err(0x2d0) // alignment error 
		case kIOReturnNotAligned:
		{	
			strncpy ( outString, "kIOReturnNotAligned", sizeof ( "kIOReturnNotAligned" ) );
		}
		break;
		
		// #define kIOReturnBadMedia        iokit_common_err(0x2d1) // Media Error 
		case kIOReturnBadMedia:
		{	
			strncpy ( outString, "kIOReturnBadMedia", sizeof ( "kIOReturnBadMedia" ) );
		}
		break;
		
		// #define kIOReturnStillOpen       iokit_common_err(0x2d2) // device(s) still open 
		case kIOReturnStillOpen:
		{	
			strncpy ( outString, "kIOReturnStillOpen", sizeof ( "kIOReturnStillOpen" ) );
		}
		break;
		
		// #define kIOReturnRLDError        iokit_common_err(0x2d3) // rld failure 
		case kIOReturnRLDError:
		{	
			strncpy ( outString, "kIOReturnRLDError", sizeof ( "kIOReturnRLDError" ) );
		}
		break;
		
		// #define kIOReturnDMAError        iokit_common_err(0x2d4) // DMA failure 
		case kIOReturnDMAError:
		{	
			strncpy ( outString, "kIOReturnDMAError", sizeof ( "kIOReturnDMAError" ) );
		}
		break;
		
		// #define kIOReturnBusy            iokit_common_err(0x2d5) // Device Busy 
		case kIOReturnBusy:
		{	
			strncpy ( outString, "kIOReturnBusy", sizeof ( "kIOReturnBusy" ) );
		}
		break;
		
		// #define kIOReturnTimeout         iokit_common_err(0x2d6) // I/O Timeout 
		case kIOReturnTimeout:
		{	
			strncpy ( outString, "kIOReturnBusy", sizeof ( "kIOReturnBusy" ) );
		}
		break;
		
		// #define kIOReturnOffline         iokit_common_err(0x2d7) // device offline 
		case kIOReturnOffline:
		{	
			strncpy ( outString, "kIOReturnOffline", sizeof ( "kIOReturnOffline" ) );
		}
		break;
		
		// #define kIOReturnNotReady        iokit_common_err(0x2d8) // not ready 
		case kIOReturnNotReady:
		{	
			strncpy ( outString, "kIOReturnNotReady", sizeof ( "kIOReturnNotReady" ) );
		}
		break;
		
		// #define kIOReturnNotAttached     iokit_common_err(0x2d9) // device not attached 
		case kIOReturnNotAttached:
		{	
			strncpy ( outString, "kIOReturnNotAttached", sizeof ( "kIOReturnNotAttached" ) );
		}
		break;
		
		// #define kIOReturnNoChannels      iokit_common_err(0x2da) // no DMA channels left
		case kIOReturnNoChannels:
		{	
			strncpy ( outString, "kIOReturnNoChannels", sizeof ( "kIOReturnNoChannels" ) );
		}
		break;
		
		// #define kIOReturnNoSpace         iokit_common_err(0x2db) // no space for data 
		case kIOReturnNoSpace:
		{	
			strncpy ( outString, "kIOReturnNoSpace", sizeof ( "kIOReturnNoSpace" ) );
		}
		break;
		
		// #define kIOReturn???Error      iokit_common_err(0x2dc) // ??? 
		// #define kIOReturnPortExists      iokit_common_err(0x2dd) // port already exists
		case kIOReturnPortExists:
		{	
			strncpy ( outString, "kIOReturnPortExists", sizeof ( "kIOReturnPortExists" ) );
		}
		break;
		
		// #define kIOReturnCannotWire      iokit_common_err(0x2de) // can't wire down physical memory
		case kIOReturnCannotWire:
		{	
			strncpy ( outString, "kIOReturnCannotWire", sizeof ( "kIOReturnCannotWire" ) );
		}
		break;
		
		// #define kIOReturnNoInterrupt     iokit_common_err(0x2df) // no interrupt attached
		case kIOReturnNoInterrupt:
		{	
			strncpy ( outString, "kIOReturnNoInterrupt", sizeof ( "kIOReturnNoInterrupt" ) );
		}
		break;
		
		// #define kIOReturnNoFrames        iokit_common_err(0x2e0) // no DMA frames enqueued
		case kIOReturnNoFrames:
		{	
			strncpy ( outString, "kIOReturnNoFrames", sizeof ( "kIOReturnNoFrames" ) );
		}
		break;
		
		// #define kIOReturnMessageTooLarge iokit_common_err(0x2e1) // oversized msg received on interrupt port
		case kIOReturnMessageTooLarge:
		{	
			strncpy ( outString, "kIOReturnMessageTooLarge", sizeof ( "kIOReturnMessageTooLarge" ) );
		}
		break;
		
		// #define kIOReturnNotPermitted    iokit_common_err(0x2e2) // not permitted
		case kIOReturnNotPermitted:
		{	
			strncpy ( outString, "kIOReturnNotPermitted", sizeof ( "kIOReturnNotPermitted" ) );
		}
		break;
		
		// #define kIOReturnNoPower         iokit_common_err(0x2e3) // no power to device
		case kIOReturnNoPower:
		{	
			strncpy ( outString, "kIOReturnNoPower", sizeof ( "kIOReturnNoPower" ) );
		}
		break;
		
		// #define kIOReturnNoMedia         iokit_common_err(0x2e4) // media not present
		case kIOReturnNoMedia:
		{	
			strncpy ( outString, "kIOReturnNoMedia", sizeof ( "kIOReturnNoMedia" ) );
		}
		break;
		
		// #define kIOReturnUnformattedMedia iokit_common_err(0x2e5)// media not formatted
		case kIOReturnUnformattedMedia:
		{	
			strncpy ( outString, "kIOReturnUnformattedMedia", sizeof ( "kIOReturnUnformattedMedia" ) );
		}
		break;
		
		// #define kIOReturnUnsupportedMode iokit_common_err(0x2e6) // no such mode
		case kIOReturnUnsupportedMode:
		{	
			strncpy ( outString, "kIOReturnUnsupportedMode", sizeof ( "kIOReturnUnsupportedMode" ) );
		}
		break;
		
		// #define kIOReturnUnderrun        iokit_common_err(0x2e7) // data underrun
		case kIOReturnUnderrun:
		{	
			strncpy ( outString, "kIOReturnUnderrun", sizeof ( "kIOReturnUnderrun" ) );
		}
		break;
		
		// #define kIOReturnOverrun         iokit_common_err(0x2e8) // data overrun
		case kIOReturnOverrun:
		{	
			strncpy ( outString, "kIOReturnOverrun", sizeof ( "kIOReturnOverrun" ) );
		}
		break;
		
		// #define kIOReturnDeviceError	 iokit_common_err(0x2e9) // the device is not working properly!
		case kIOReturnDeviceError:
		{	
			strncpy ( outString, "kIOReturnDeviceError", sizeof ( "kIOReturnDeviceError" ) );
		}
		break;
	
		// #define kIOReturnAborted	 iokit_common_err(0x2eb) // operation aborted
		case kIOReturnAborted:
		{	
			strncpy ( outString, "kIOReturnAborted", sizeof ( "kIOReturnAborted" ) );
		}
		break;
		
		// #define kIOReturnNoBandwidth	 iokit_common_err(0x2ec) // bus bandwidth would be exceeded
		case kIOReturnNoBandwidth:
		{	
			strncpy ( outString, "kIOReturnNoBandwidth", sizeof ( "kIOReturnNoBandwidth" ) );
		}
		break;
		
		// #define kIOReturnNotResponding	 iokit_common_err(0x2ed) // device not responding
		case kIOReturnNotResponding:
		{	
			strncpy ( outString, "kIOReturnNoBandwidth", sizeof ( "kIOReturnNoBandwidth" ) );
		}
		break;
		
		// #define kIOReturnIsoTooOld	 iokit_common_err(0x2ee) // isochronous I/O request for distant past!
		case kIOReturnIsoTooOld:
		{	
			strncpy ( outString, "kIOReturnIsoTooOld", sizeof ( "kIOReturnIsoTooOld" ) );
		}
		break;
		
		// #define kIOReturnIsoTooNew	 iokit_common_err(0x2ef) // isochronous I/O request for distant future
		case kIOReturnIsoTooNew:
		{	
			strncpy ( outString, "kIOReturnIsoTooNew", sizeof ( "kIOReturnIsoTooNew" ) );
		}
		break;
		
		// #define kIOReturnNotFound        iokit_common_err(0x2f0) // data was not found
		case kIOReturnNotFound:
		{	
			strncpy ( outString, "kIOReturnNotFound", sizeof ( "kIOReturnNotFound" ) );
		}
		break;
		
		// #define kIOReturnInvalid         iokit_common_err(0x1)   // should never be seen
		case kIOReturnInvalid:
		{	
			strncpy ( outString, "kIOReturnInvalid", sizeof ( "kIOReturnInvalid" ) );
		}
		break;
	
		default:
		{
			strncpy ( outString, "NO STRING", sizeof ( "NO STRING" ) );
		}
		break;
	
	}
	
	returnValue = TRUE;
	
	
ErrorExit:

	return returnValue;

}


//-----------------------------------------------------------------------------
//	StringFromUSBReturn
//-----------------------------------------------------------------------------

static boolean_t
StringFromUSBReturn ( unsigned ioReturnCode, char * outString )
{

	boolean_t returnValue = FALSE;
	
	switch ( ioReturnCode )
	{
	
		// #define kIOUSBUnknownPipeErr        iokit_usb_err(0x61)			// 0xe0004061  Pipe ref not recognized
		case kIOUSBUnknownPipeErr:
		{	
			strncpy ( outString, "kIOUSBUnknownPipeErr", sizeof ( "kIOUSBUnknownPipeErr" ) );
		}
		break;
		
		// #define kIOUSBTooManyPipesErr       iokit_usb_err(0x60)			// 0xe0004060  Too many pipes
		case kIOUSBTooManyPipesErr:
		{	
			strncpy ( outString, "kIOUSBTooManyPipesErr", sizeof ( "kIOUSBTooManyPipesErr" ) );
		}
		break;
		
		// #define kIOUSBNoAsyncPortErr        iokit_usb_err(0x5f)			// 0xe000405f  no async port
		case kIOUSBNoAsyncPortErr:
		{	
			strncpy ( outString, "kIOUSBNoAsyncPortErr", sizeof ( "kIOUSBNoAsyncPortErr" ) );
		}
		break;
		
		// #define kIOUSBNotEnoughPipesErr     iokit_usb_err(0x5e)			// 0xe000405e  not enough pipes in interface
		case kIOUSBNotEnoughPipesErr:
		{	
			strncpy ( outString, "kIOUSBNotEnoughPipesErr", sizeof ( "kIOUSBNotEnoughPipesErr" ) );
		}
		break;
		
		// #define kIOUSBNotEnoughPowerErr     iokit_usb_err(0x5d)			// 0xe000405d  not enough power for selected configuration
		case kIOUSBNotEnoughPowerErr:
		{	
			strncpy ( outString, "kIOUSBNotEnoughPowerErr", sizeof ( "kIOUSBNotEnoughPowerErr" ) );
		}
		break;
		
		// #define kIOUSBEndpointNotFound      iokit_usb_err(0x57)			// 0xe0004057  Endpoint Not found
		case kIOUSBEndpointNotFound:
		{	
			strncpy ( outString, "kIOUSBEndpointNotFound", sizeof ( "kIOUSBEndpointNotFound" ) );
		}
		break;
		
		// #define kIOUSBConfigNotFound        iokit_usb_err(0x56)			// 0xe0004056  Configuration Not found
		case kIOUSBConfigNotFound:
		{	
			strncpy ( outString, "kIOUSBConfigNotFound", sizeof ( "kIOUSBConfigNotFound" ) );
		}
		break;
		
		// #define kIOUSBTransactionTimeout    iokit_usb_err(0x51)			// 0xe0004051  Transaction timed out
		case kIOUSBTransactionTimeout:
		{	
			strncpy ( outString, "kIOUSBTransactionTimeout", sizeof ( "kIOUSBTransactionTimeout" ) );
		}
		break;
		
		// #define kIOUSBTransactionReturned   iokit_usb_err(0x50)			// 0xe0004050  The transaction has been returned to the caller
		case kIOUSBTransactionReturned:
		{	
			strncpy ( outString, "kIOUSBTransactionReturned", sizeof ( "kIOUSBTransactionReturned" ) );
		}
		break;
		
		// #define kIOUSBPipeStalled           iokit_usb_err(0x4f)			// 0xe000404f  Pipe has stalled, error needs to be cleared
		case kIOUSBPipeStalled:
		{	
			strncpy ( outString, "kIOUSBPipeStalled", sizeof ( "kIOUSBPipeStalled" ) );
		}
		break;
		
		// #define kIOUSBInterfaceNotFound     iokit_usb_err(0x4e)			// 0xe000404e  Interface ref not recognized
		case kIOUSBInterfaceNotFound:
		{	
			strncpy ( outString, "kIOUSBInterfaceNotFound", sizeof ( "kIOUSBInterfaceNotFound" ) );
		}
		break;
		
		// #define kIOUSBLowLatencyBufferNotPreviouslyAllocated        iokit_usb_err(0x4d)			// 0xe000404d  Attempted to use user land low latency isoc calls w/out calling PrepareBuffer (on the data buffer) first 
		case kIOUSBLowLatencyBufferNotPreviouslyAllocated:
		{	
			strncpy ( outString, "kIOUSBLowLatencyBufferNotPreviouslyAllocated", sizeof ( "kIOUSBLowLatencyBufferNotPreviouslyAllocated" ) );
		}
		break;

		// #define kIOUSBLowLatencyFrameListNotPreviouslyAllocated     iokit_usb_err(0x4c)			// 0xe000404c  Attempted to use user land low latency isoc calls w/out calling PrepareBuffer (on the frame list) first
		case kIOUSBLowLatencyFrameListNotPreviouslyAllocated:
		{	
			strncpy ( outString, "kIOUSBLowLatencyFrameListNotPreviouslyAllocated", sizeof ( "kIOUSBLowLatencyFrameListNotPreviouslyAllocated" ) );
		}
		break;
		
		// #define kIOUSBHighSpeedSplitError     iokit_usb_err(0x4b)		// 0xe000404b Error to hub on high speed bus trying to do split transaction
		case kIOUSBHighSpeedSplitError:
		{	
			strncpy ( outString, "kIOUSBHighSpeedSplitError", sizeof ( "kIOUSBHighSpeedSplitError" ) );
		}
		break;

		// #define kIOUSBSyncRequestOnWLThread	iokit_usb_err(0x4a)			// 0xe000404a  A synchronous USB request was made on the workloop thread (from a callback?).  Only async requests are permitted in that case
		case kIOUSBSyncRequestOnWLThread:
		{	
			strncpy ( outString, "kIOUSBSyncRequestOnWLThread", sizeof ( "kIOUSBSyncRequestOnWLThread" ) );
		}
		break;

		// #define kIOUSBDeviceNotHighSpeed	iokit_usb_err(0x49)			// 0xe0004049  The device is not a high speed device, so the EHCI driver returns an error
		case kIOUSBDeviceNotHighSpeed:
		{	
			strncpy ( outString, "kIOUSBDeviceNotHighSpeed", sizeof ( "kIOUSBDeviceNotHighSpeed" ) );
		}
		break;

		// #define kIOUSBLinkErr           iokit_usb_err(0x10)		// 0xe0004010
		case kIOUSBLinkErr:
		{	
			strncpy ( outString, "kIOUSBLinkErr", sizeof ( "kIOUSBLinkErr" ) );
		}
		break;
		
		// #define kIOUSBNotSent2Err       iokit_usb_err(0x0f)		// 0xe000400f Transaction not sent
		case kIOUSBNotSent2Err:
		{	
			strncpy ( outString, "kIOUSBNotSent2Err", sizeof ( "kIOUSBNotSent2Err" ) );
		}
		break;
		
		// #define kIOUSBNotSent1Err       iokit_usb_err(0x0e)		// 0xe000400e Transaction not sent
		case kIOUSBNotSent1Err:
		{	
			strncpy ( outString, "kIOUSBNotSent1Err", sizeof ( "kIOUSBNotSent1Err" ) );
		}
		break;
		
		// #define kIOUSBBufferUnderrunErr iokit_usb_err(0x0d)		// 0xe000400d Buffer Underrun (Host hardware failure on data out, PCI busy?)
		case kIOUSBBufferUnderrunErr:
		{	
			strncpy ( outString, "kIOUSBBufferUnderrunErr", sizeof ( "kIOUSBBufferUnderrunErr" ) );
		}
		break;

		// #define kIOUSBBufferOverrunErr  iokit_usb_err(0x0c)		// 0xe000400c Buffer Overrun (Host hardware failure on data out, PCI busy?)
		case kIOUSBBufferOverrunErr:
		{	
			strncpy ( outString, "kIOUSBBufferOverrunErr", sizeof ( "kIOUSBBufferOverrunErr" ) );
		}
		break;
		
		// #define kIOUSBReserved2Err      iokit_usb_err(0x0b)		// 0xe000400b Reserved
		case kIOUSBReserved2Err:
		{	
			strncpy ( outString, "kIOUSBReserved2Err", sizeof ( "kIOUSBReserved2Err" ) );
		}
		break;
		
		// #define kIOUSBReserved1Err      iokit_usb_err(0x0a)		// 0xe000400a Reserved
		case kIOUSBReserved1Err:
		{	
			strncpy ( outString, "kIOUSBReserved1Err", sizeof ( "kIOUSBReserved1Err" ) );
		}
		break;
		
		// #define kIOUSBWrongPIDErr       iokit_usb_err(0x07)		// 0xe0004007 Pipe stall, Bad or wrong PID
		case kIOUSBWrongPIDErr:
		{	
			strncpy ( outString, "kIOUSBWrongPIDErr", sizeof ( "kIOUSBWrongPIDErr" ) );
		}
		break;
		
		// #define kIOUSBPIDCheckErr       iokit_usb_err(0x06)		// 0xe0004006 Pipe stall, PID CRC error
		case kIOUSBPIDCheckErr:
		{	
			strncpy ( outString, "kIOUSBPIDCheckErr", sizeof ( "kIOUSBPIDCheckErr" ) );
		}
		break;
		
		// #define kIOUSBDataToggleErr     iokit_usb_err(0x03)		// 0xe0004003 Pipe stall, Bad data toggle
		case kIOUSBDataToggleErr:
		{	
			strncpy ( outString, "kIOUSBDataToggleErr", sizeof ( "kIOUSBDataToggleErr" ) );
		}
		break;
		
		// #define kIOUSBBitstufErr        iokit_usb_err(0x02)		// 0xe0004002 Pipe stall, bitstuffing
		case kIOUSBBitstufErr:
		{	
			strncpy ( outString, "kIOUSBBitstufErr", sizeof ( "kIOUSBBitstufErr" ) );
		}
		break;
		
		// #define kIOUSBCRCErr            iokit_usb_err(0x01)		// 0xe0004001 Pipe stall, bad CRC
		case kIOUSBCRCErr:
		{	
			strncpy ( outString, "kIOUSBCRCErr", sizeof ( "kIOUSBCRCErr" ) );
		}
		break;
	
		default:
		{
			strncpy ( outString, "NO STRING", sizeof ( "NO STRING" ) );
		}
		break;
	
	}
	
	returnValue = TRUE;
	
	
ErrorExit:

	return returnValue;

}

//-----------------------------------------------------------------------------
//	EOF
//-----------------------------------------------------------------------------
