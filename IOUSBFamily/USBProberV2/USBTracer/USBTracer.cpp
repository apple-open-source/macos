/*
 * Copyright © 2009 Apple Inc.  All rights reserved.
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

#include "USBTracer.h"


//—————————————————————————————————————————————————————————————————————————————
//	Globals
//—————————————————————————————————————————————————————————————————————————————

int					gBiasSeconds				= 0;
double				gDivisor					= 1000.0;		/* Trace divisor converts to microseconds */
kd_buf *			gTraceBuffer				= NULL;
boolean_t			gTraceEnabled				= FALSE;
boolean_t			gSetRemoveFlag				= TRUE;
const char *		gProgramName				= NULL;
uint32_t			gPrintMask					= 0xFFFFFFFF - kPrintMaskAllTracepoints;	
boolean_t			gPrintCodes					= FALSE;
boolean_t			gPrintCPU					= FALSE;
boolean_t			gPrintThread				= FALSE;
boolean_t			gVerbose					= FALSE;
uint32_t			gTimeStampMask				= kTimeStampKernel;
char				gMethodString[128];	
boolean_t			gPrintGroup					= TRUE;
boolean_t			gPrintMethod				= TRUE;
boolean_t			gPrintUSBP					= TRUE;
boolean_t			gPrintRHTimerFired			= FALSE;
int					gNumIndentTabs				= 0;
boolean_t			gPrintIndent				= FALSE;
uint32_t			gTraceBufferSize			= kTraceBufferSampleSize;
uint32_t			gSavedFWDebugMask			= 0;
boolean_t			gEnableUSBTrace				= TRUE;
boolean_t			gDisableUSBLogging			= FALSE;
boolean_t			gOuputRaw					= FALSE;
boolean_t			gShouldReadRawFile			= FALSE;
char				gLogFilePath[kFilePathMaxSize];
FILE *				gLogFileStream				= NULL;
char				gCodesFilePath[kFilePathMaxSize];
FILE *				gCodesFileStream			= NULL;
unsigned int		gRegTypeInterest			= KDBG_RANGETYPE;
unsigned int		gRegTypeValue1				= 0;
unsigned int		gRegTypeValue2				= -1;
boolean_t			gBasicFormatting			= FALSE;
uint64_t			gStartingAbsTime			= 0;
uint64_t			gLastTimeStamp				= -1;
boolean_t			gPrintNoSep					= FALSE;
uint32_t			gSavedUSBDebugMask			= 0;
boolean_t			gDump						= FALSE;

static void PrintBufferSettings ( void )
{
	kbufinfo_t bufinfo = { 0, 0, 0, 0, 0 };
	GetTraceBufferInfo( &bufinfo );
	
	printf("The kernel buffer settings are:\n");
	printf("\tentries:\t%d\n", bufinfo.nkdbufs);	// nkdbufs isn't the actual buffer size, only what's been set. Call reinit to actually use.
	printf("\tflags:\t\t0x%x\n", bufinfo.flags);
	printf("\tnolog:\t\t%d\n", bufinfo.nolog);
	printf("\tbufid:\t\t%d\n", bufinfo.bufid);
	printf("\tthreads:\t%d\n", bufinfo.nkdthreads);
}

#pragma mark Functions
//———————————————————————————————————————————————————————————————————————————
//	Main
//———————————————————————————————————————————————————————————————————————————

int
main ( int argc, const char * argv[] )
{
	
	gProgramName = argv[0];
	
	if ( geteuid ( ) != 0 )
	{
		
		elog( "'%s' must be run as root.\n", gProgramName );
		exit ( 1 );
		
	}
	
    if ( reexec_to_match_kernel() )
	{
		elog( "Could not re-execute to match kernel architecture. (Error %d)\n", errno );
		exit( 1 );
    }

	// Get program arguments.
	ParseArguments ( argc, argv );
	
	// Collect trace into auto-allocated buffer
	if ( gDump )
	{
		CollectWithAlloc();
		exit(0);
	}
	
	// Set up signal handlers.
	RegisterSignalHandlers ( );	
	
	if ( gShouldReadRawFile )
	{
		ReadRawFile(gLogFilePath);
		exit( 0 );
	}
	
	EnableUSBTracing ( );
	
	// Allocate trace buffer.
	AllocateTraceBuffer ( );
	
	// Remove the trace buffer.
	RemoveTraceBuffer ( );
	
	// Set the new trace buffer size. Must Reinit *after*.
	SetTraceBufferSize ( gTraceBufferSize );
	
#if 1
	// Initialize the trace buffer.
	SetInterest ( gRegTypeInterest );
	
	// Reinitialize the facility.
	Reinitialize ( );
#else
	// Initialize the trace buffer.
	InitializeTraceBuffer ( );
	
	// Enable the trace buffer.
	EnableTraceBuffer ( 1 );
#endif	
	// Get the clock divisor.
	GetDivisor ( );
	
	// Print kernel buffer settings if verbose.
	if ( gVerbose )
		PrintBufferSettings();
	
	// Enable the trace buffer.
	EnableTraceBuffer ( 1 );
	
	// Main loop
	if ( !gDisableUSBLogging )
	{
		if ( gOuputRaw )
		{
			if ( (gLogFileStream = fopen( gLogFilePath, "wb+")) )
			{
				PrependDivisorEntry( gLogFileStream );
				
				while ( 1 )
				{
					CollectToRawFile( gLogFileStream );
					usleep ( kMicrosecondsPerCollectionDelay * kMicrosecondsPerMillisecond );
				}
				
				// file handle closed in Quit()
			}
			else
				Quit("Can't open file!\n");
		}
		else
		{
			while ( 1 )
			{
				CollectTrace ( );
				usleep ( kMicrosecondsPerCollectionDelay * kMicrosecondsPerMillisecond );
			}
		}
	}
	
}


//———————————————————————————————————————————————————————————————————————————
//	PrintUsage
//———————————————————————————————————————————————————————————————————————————

static void
PrintUsage ( void )
{
	elog ( "\n");
	elog ( "Usage: sudo %s [OPTIONS]\n", gProgramName );
	elog ( "\n");
	
	elog ( "OPTIONS\n");
	elog ( "\tThe available options are as follows:\n");
	elog ( "\n");
	
	elog ( "\t--bufsize=size, -b size\n");
	elog ( "\t\t Set the trace buffer to hold size number of trace points.\n");
	
	elog ( "\t--all, -a\n");
	elog ( "\t\t Show all tracepoint codes, even if they aren't USB codes.\n");
	
	elog ( "\t--decode[=path-to-code-file], -d [path-to-code-file]\n");
	elog ( "\t\t Try to decode unknown tracepoint codes with file. Defaults to '%s'. Implies --all.\n", kKernelTraceCodes );
	
	elog ( "\t--codes, -c\n");
	elog ( "\t\t Print tracepoint codes.\n");
	
	elog ( "\t--time, -t\n");
	elog ( "\t\t Print the local system time (hh:mm) with each log.\n");
	
	elog ( "\t--nostamp, -s\n");
	elog ( "\t\t Do not print the timestamp of each tracepoint. This option also disables -t.\n");
	
	elog ( "\t--nogroup, -g\n");
	elog ( "\t\t Do not print the group string.\n");
	
	elog ( "\t--nomethod, -m\n");
	elog ( "\t\t Do not print the method string.\n");
	
	elog ( "\t--nousbp, -u\n");
	elog ( "\t\t Do NOT print the USB object pointer, if it exists.\n");
	
	elog ( "\t--timers, -f\n");
	elog ( "\t\t Print the log everytime the root hub timer fires.\n");
	
	elog ( "\t--indent, -i\n");
	elog ( "\t\t Indent the logs.\n");
	
	elog ( "\t--notrace, -T\n");
	elog ( "\t\t Do not enable USB tracing.\n");
	
	elog ( "\t--nolog, -L\n");
	elog ( "\t\t Disable printing of USB tracepoint logging.\n");
	
	elog ( "\t--write[=path-to-file], -w [path-to-file]\n");
	elog ( "\t\t Log codes in raw format\n");
	
	elog ( "\t--read=path-to-file, -r path-to-file\n");
	elog ( "\t\t Read tracepoints from a raw format file\n");	
	
	elog ( "\t--cpu, -C\n");
	elog ( "\t\t Display CPU numbers.\n");
	
	elog ( "\t--thread, -H\n");
	elog ( "\t\t Display thread.\n");
	
	elog ( "\t--interest=type, -I type\n");
	elog ( "\t\t Set the interest to type. Defaults to KDBG_RANGETYPE.\n");
	
	elog ( "\t--value1=value, -1 value\n");
	elog ( "\t\t Sets interest type value 1 to value. Only valid with --interest.\n");
	
	elog ( "\t--value2=value, -2 value\n");
	elog ( "\t\t Sets interest type value 2 to value. Only valid with --interest.\n");
	
	elog ( "\t--basic, -B\n");
	elog ( "\t\t Use basic formatting, similar to trace.\n");
	
	elog ( "\t--nosep, -S\n");
	elog ( "\t\t Use non-separating format between method and log.\n");
	
	elog ( "\t--dump, -D\n");
	elog ( "\t\t Read and print the trace buffer using an auto-allocated collection buffer, then exit.\n");
	
	elog ( "\t--verbose, -v\n");
	elog ( "\t\t Verbose mode.\n");
	
	elog ( "\t--version, -V\n");
	elog ( "\t\t Print version.\n");
	
	elog ( "\t--help, -h, -?\n");
	elog ( "\t\t Show this help.\n");
	
	exit ( 0 );
	
}


//———————————————————————————————————————————————————————————————————————————
//	ParseArguments
//———————————————————————————————————————————————————————————————————————————

static void
ParseArguments ( int argc, const char * argv[] )
{
	int 					c;
	struct option 			long_options[] =
	{
		{ "bufsize",		required_argument,	0, 'b' },
		{ "buffer",			required_argument,	0, 'b' },
		{ "size",			required_argument,	0, 'b' },
		{ "all",			no_argument,		0, 'a' },
		{ "decode",			optional_argument,	0, 'd' },
		{ "codes",			no_argument,		0, 'c' },
		{ "time",			no_argument,		0, 't' },
		{ "nostamp",		no_argument,		0, 's' },
		{ "nogroup",		no_argument,		0, 'g' },
		{ "nomethod",		no_argument,		0, 'm' },
		{ "nousbp",			no_argument,		0, 'u' },
		{ "timers",			no_argument,		0, 'f' },
		{ "indent",			no_argument,		0, 'i' },
		{ "notrace",		no_argument,		0, 'T' },
		{ "nolog",			no_argument,		0, 'L' },
		{ "write",			optional_argument,	0, 'w' },
		{ "read",			required_argument,	0, 'r' },
		{ "cpu",			no_argument,		0, 'C' },
		{ "thread",			no_argument,		0, 'H' },
		{ "interest",		required_argument,	0, 'I' },
		{ "value1",			required_argument,	0, '1' },
		{ "value2",			required_argument,	0, '2' },
		{ "basic",			no_argument,		0, 'B' },
		{ "nosep",			no_argument,		0, 'S' },
		{ "dump",			no_argument,		0, 'D' },
		
		{ "verbose",		no_argument,		0, 'v' },
		{ "version",		no_argument,		0, 'V' },
		{ "help",			no_argument,		0, 'h' },
		{ 0, 0, 0, 0 }
	};
	
	if ( argc == 1 )
	{
		return;
	}
	
    while ( ( c = getopt_long ( argc, ( char * const * ) argv , "b:ad::ctsgmufiTLw::r:CHI:1:2:BSDvVh?", long_options, NULL  ) ) != -1 )
	{
		switch ( c )
		{
			case 'b':
				gTraceBufferSize = (uint32_t)strtoul(optarg, NULL, 0);	// is this safe?
				if ( gTraceBufferSize < 1 )
				{
					elog( "An invalid buffer of size %u was requested!\n", gTraceBufferSize);
					Quit("Argument parsing error\n");
				}
				vlog( "Setting trace buffer size to %u\n", gTraceBufferSize );
				break;
				
			case 'a':
				gPrintMask |= kPrintMaskAllTracepoints;
				vlog( "Setting print mask to 0x%08x\n", gPrintMask );
				break;
				
			case 'd':
				if (optarg != NULL)
				{
					if ( strlcpy(gCodesFilePath, optarg, sizeof(gCodesFilePath)) >= sizeof(gCodesFilePath) )
						Quit( "File path length of decode file is too long\n");
				}
				else
				{
					// need to decode all other tracepoints using /usr/local/share/misc/trace.codes
					if ( strlcpy(gCodesFilePath, kKernelTraceCodes, sizeof(gCodesFilePath)) >= sizeof(gCodesFilePath) )
						Quit( "File path length of decode file is too long\n");
				}
				
				gCodesFileStream = fopen( gCodesFilePath, "r");
				if ( gCodesFileStream )
				{
					gPrintMask |= kPrintMaskAllTracepoints;
					vlog( "Using '%s' file to decode other tracepoints\n", gCodesFilePath );
				}
				else
				{
					elog( "There was a problem opening the file at '%s'\n", gCodesFilePath);
				}
				break;
				
			case 'c':
				gPrintCodes = TRUE;	
				vlog( "Will display codes\n");
				break;
			
			case 't':
				gTimeStampMask |= kTimeStampLocalTime;
				vlog( "Will display local time\n");
				break;
				
			case 's':
				gTimeStampMask &= (0xffffffff - kTimeStampKernel);
				vlog( "Will NOT display time stamps\n");
				break;
				
			case 'g':
				gPrintGroup = FALSE;
				vlog( "Will NOT display group name\n");
				break;
				
			case 'm':
				gPrintMethod = FALSE;
				vlog( "Will NOT display method name\n");
				break;
				
			case 'u':
				gPrintUSBP = FALSE;
				vlog( "Will NOT display USB pointer\n");
				break;
				
			case 'f':
				gPrintRHTimerFired = TRUE;
				vlog( "Will print the RootHubTimer fired messages\n");
				break;
				
			case 'i':
				gPrintIndent = TRUE;
				vlog( "Will indent\n");
				break;
				
			case 'T':
				gEnableUSBTrace = FALSE;
				vlog( "Will NOT enable USB tracing\n");
				break;
				
			case 'L':
				gDisableUSBLogging = TRUE;
				vlog( "Will disable printing of USB tracepoint logging\n");
				break;
				
			case 'w':
				vlog("Got w flag\n");
				if (optarg != NULL)
				{
					if ( strlcpy(gLogFilePath, optarg, sizeof(gLogFilePath)) >= sizeof(gLogFilePath) )
						Quit( "The path length of raw file is too long\n");
				}
				else
				{
					char timestring[30];
					time_t currentTime = time ( NULL );
					strftime( timestring, 30, "./raw-%y%d%m%H%M%S", localtime(&currentTime) );
					
					if ( strlcpy(gLogFilePath, timestring, sizeof(gLogFilePath)) >= sizeof(gLogFilePath) )
						Quit( "The path length of raw file is too long\n");
				}
				
				gOuputRaw = TRUE;
				vlog( "Will output raw format file to '%s'\n", gLogFilePath );
				break;
				
			case 'r':
				if ( strlcpy(gLogFilePath, optarg, sizeof(gLogFilePath)) >= sizeof(gLogFilePath) )
					Quit( "File path length of raw data file is too long\n");
				
				gShouldReadRawFile = TRUE;	
				vlog( "Will read raw data from file %s\n", gLogFilePath );
				break;
				
			case 'C':
				gPrintCPU = TRUE;	
				vlog( "Will display CPU number\n");
				break;
				
			case 'H':
				gPrintThread = TRUE;	
				vlog( "Will display thread\n");
				break;
				
			case 'I':
				gRegTypeInterest = (uint32_t)strtoul(optarg, NULL, 0);	// is this safe?
				vlog( "Setting interest to 0x%x\n", gRegTypeInterest );
				break;
				
			case '1':
				gRegTypeValue1 = (uint32_t)strtoul(optarg, NULL, 0);	// is this safe?
				vlog( "Setting interest value 1 to %u\n", gRegTypeValue1 );
				break;
				
			case '2':
				gRegTypeValue2 = (uint32_t)strtoul(optarg, NULL, 0);	// is this safe?
				vlog( "Setting interest value 2 to %u\n", gRegTypeValue2 );
				break;
				
			case 'B':
				gBasicFormatting = TRUE;
				vlog( "Will using basic formatting\n");
				break;
				
			case 'S':
				gPrintNoSep = TRUE;
				vlog( "Will NOT separate log from method during formatting.\n");
				break;
				
			case 'D':
				gDump = TRUE;
				vlog( "Will dump trace buffer and exit.\n");
				break;
				
			case 'v':
				gVerbose = TRUE;
				vlog( "Verbose mode ON\n");
				break;
				
			case 'V':
				fprintf(stdout,"usbtracer version:  %s\n", QUOTEDSTRING(USBTRACE_VERSION));
				break;
			
			case 'h':
				PrintUsage ( );
				break;
				
			case '?':
				PrintUsage ( );
				break;
				
			default:
				break;
		}
	}
}


//———————————————————————————————————————————————————————————————————————————
//	EnableUSBWireTracing
//———————————————————————————————————————————————————————————————————————————

static void
EnableUSBTracing ( void )
{	
	int error;
	USBSysctlArgs usbArgs;
	
	if ( !gEnableUSBTrace )
	{
		vlog( "Will not enable USB tracing.\n");
		return;
	}
	
	bzero ( &usbArgs, sizeof ( usbArgs ) );
	
	usbArgs.type 		= kUSBTypeDebug;
	usbArgs.operation 	= kUSBOperationGetFlags;
	
	error = sysctlbyname ( USB_SYSCTL, NULL, NULL, &usbArgs, sizeof ( usbArgs ) );
	if ( error != 0 )
	{
		//LoadExtension ( );
		
		error = sysctlbyname ( USB_SYSCTL, NULL, NULL, &usbArgs, sizeof ( usbArgs ) );
		if ( error != 0 )
		{
			elog( "sysctlbyname failed to get old 'usb' debug flags second time\n");
		}
		
	}
	
	gSavedUSBDebugMask = usbArgs.debugFlags;	
	
	usbArgs.type 		= kUSBTypeDebug;
	usbArgs.operation 	= kUSBOperationSetFlags;
	usbArgs.debugFlags 	= usbArgs.debugFlags | kUSBEnableTracePointsMask;
	
	error = sysctlbyname ( USB_SYSCTL, NULL, NULL, &usbArgs, sizeof ( usbArgs ) );
	if ( error != 0 )
	{
		elog( "sysctlbyname failed to set new 'usb' debug flags\n");
	}
}


//———————————————————————————————————————————————————————————————————————————
//	RegisterSignalHandlers
//———————————————————————————————————————————————————————————————————————————

static void
RegisterSignalHandlers ( void )
{
	
	signal ( SIGINT, SignalHandler );
	signal ( SIGQUIT, SignalHandler );
	signal ( SIGHUP, SignalHandler );
	signal ( SIGTERM, SignalHandler );
	
}


//———————————————————————————————————————————————————————————————————————————
//	AllocateTraceBuffer
//———————————————————————————————————————————————————————————————————————————

static void
AllocateTraceBuffer ( void )
{
	
	gTraceBuffer = ( kd_buf * ) malloc ( gTraceBufferSize * sizeof ( kd_buf ) );
	if ( gTraceBuffer == NULL )
	{
		Quit ( "Can't allocate memory for tracing info\n");
	}
}


//———————————————————————————————————————————————————————————————————————————
//	SignalHandler
//———————————————————————————————————————————————————————————————————————————

static void
SignalHandler ( int signal )
{
	
	vlog( "Quiting on signal %d\n", signal );
	
	Quit( NULL );
	
}



//———————————————————————————————————————————————————————————————————————————
//	EnableTraceBuffer
//———————————————————————————————————————————————————————————————————————————

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
		Quit ( "trace facility failure, KERN_KDENABLE\n");
	
	if ( val )
		gTraceEnabled = TRUE;
	else
		gTraceEnabled = FALSE;
}


//———————————————————————————————————————————————————————————————————————————
//	SetTraceBufferSize
//———————————————————————————————————————————————————————————————————————————

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
		Quit ( "trace facility failure, KERN_KDSETBUF\n");
}


//———————————————————————————————————————————————————————————————————————————
//	GetTraceBufferInfo
//———————————————————————————————————————————————————————————————————————————

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
		Quit ( "trace facility failure, KERN_KDGETBUF\n");
}


//———————————————————————————————————————————————————————————————————————————
//	RemoveTraceBuffer
//———————————————————————————————————————————————————————————————————————————

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
			Quit ( "The trace facility is currently in use.\n");
		else
			Quit ( "Trace facility failure, KERN_KDREMOVE\n");
	}
}

//———————————————————————————————————————————————————————————————————————————
//	Reinitialize the facility
//———————————————————————————————————————————————————————————————————————————

static void
Reinitialize ( void )
{
	int 		mib[6];
	size_t 		needed;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					// no flags 
	
	if ( sysctl ( mib, 3, NULL, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETUP\n");
}

//———————————————————————————————————————————————————————————————————————————
//	SetInterest
//———————————————————————————————————————————————————————————————————————————

static void
SetInterest ( unsigned int type ) 
{
	int 		mib[6];
	size_t 		needed;
	kd_regtype	kr;
	
	switch ( type )
	{
		case KDBG_SUBCLSTYPE:
			kr.type 	= KDBG_SUBCLSTYPE;	// interested in a specific subclass
			kr.value1 	= gRegTypeValue1;	// class
			kr.value2	= gRegTypeValue2;	// subclass
			break;
		
		case KDBG_VALCHECK:
			kr.type 	= KDBG_VALCHECK;	// interested in a specific code
			kr.value1 	= gRegTypeValue1;	// code
			kr.value2	= gRegTypeValue2;	// code
			kr.value3 	= 0;
			kr.value4	= 0;
			break;
			
		case KDBG_RANGETYPE:	// default to RangeType
		case KDBG_CLASSTYPE:
		case KDBG_CKTYPES:
		case KDBG_TYPENONE:
		default:
			kr.type 	= KDBG_RANGETYPE;	// interested in range of codes
			kr.value1 	= gRegTypeValue1;	// starting at value1
			kr.value2	= gRegTypeValue2;	// and ending at value2
			break;
	}
	
	needed = sizeof ( kd_regtype );
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	if ( sysctl ( mib, 3, &kr, &needed, NULL, 0 ) < 0 )
		Quit ( "trace facility failure, KERN_KDSETREG\n");
}


#if 0
//———————————————————————————————————————————————————————————————————————————
//	InitializeTraceBuffer
//———————————————————————————————————————————————————————————————————————————

static void
InitializeTraceBuffer ( void ) 
{
	
	int 		mib[6];
	size_t 		needed;
	kd_regtype	kr;
	int			err = 0;
	
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
	
	err = sysctl ( mib, 3, &kr, &needed, NULL, 0 );
	
	if (err == -1) {
		err = errno;
	}
	
	if (err != 0) 
	{
		fprintf(stderr, "sysctl KERN_KDSETREG error:  %s (%d)\n", strerror(err),err);
		Quit ( "trace facility failure, KERN_KDSETREG\n");
	}
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;					/* no flags */
	
	err = sysctl ( mib, 3, NULL, &needed, NULL, 0 );
	
	if (err == -1) {
		err = errno;
	}
	
	if (err != 0) 
	{
		fprintf(stderr, "sysctl KERN_KDSETUP error:  %s (%d)\n", strerror(err),err);
		Quit ( "trace facility failure, KERN_KDSETUP\n");
	}
}
#endif

//———————————————————————————————————————————————————————————————————————————
//	CollectTrace
//———————————————————————————————————————————————————————————————————————————

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
	
	int error = sysctl ( mib, 3, gTraceBuffer, &needed, NULL, 0 );
	if ( error < 0 )
	{
		vlog ("CollectTrace error %d\n", error );
		Quit ( "trace facility failure, KERN_KDREADTR\nTry using a larger buffer size with --bufsize.\n");
	}
	count = (int) needed;
	//vlog("Read %u trace entries, %lu bytes\n", count, count * sizeof(kd_buf) );
	
	if ( bufinfo.flags & KDBG_WRAPPED )
	{
		EnableTraceBuffer ( 0 );
		EnableTraceBuffer ( 1 );
		vlog( "Buffer has wrapped.\n");
	}
	
	for ( index = 0; index < count; index++ )
	{
		ProcessTracepoint( gTraceBuffer[index] );
	}
	
	fflush ( 0 );
}	

//———————————————————————————————————————————————————————————————————————————
//	CollectWithAlloc
//———————————————————————————————————————————————————————————————————————————

static void
CollectWithAlloc( void )
{
	char *			buffer;
	kd_buf *		kd;
	int				reenable = 0;
	int				mib[6];
	size_t 			needed;
	int 			index;
	kbufinfo_t		bufinfo = {0, 0, 0, 0};
	
	// Get kernel buffer information
	GetTraceBufferInfo(&bufinfo);
	
	if (bufinfo.nolog != 1)
	{
		reenable = 1;
		EnableTraceBuffer(0);  // disable logging
	}
	
	buffer = (char *)(malloc(bufinfo.nkdbufs * sizeof(kd_buf)));
	if ( buffer == (char *) 0 )
		Quit("can't allocate memory for tracing info\n");
	
	needed = bufinfo.nkdbufs * sizeof(kd_buf);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	
	int error = sysctl(mib, 3, buffer, &needed, NULL, 0);
	if ( error < 0)
	{
		vlog ("CollectWithAlloc error %d\n", error );
		Quit("trace facility failure, KERN_KDREADTR\n");
	}
	
	if (reenable == 1)
	{
		EnableTraceBuffer(1);  // re-enable kernel logging
	}
	
	if (!needed)
		Quit("trace facility failure, KERN_KDREADTR (no data) \n");
	
	kd = (kd_buf *) buffer;
	
	for ( index = 0; index < needed; index++ )
	{
		ProcessTracepoint( kd[index] );
	}
	
	fflush ( 0 );
}

//———————————————————————————————————————————————————————————————————————————
//	ProcessTracepoint
//———————————————————————————————————————————————————————————————————————————

static void
ProcessTracepoint( kd_buf tracepoint )
{
	uint32_t 			debugID;
	uint32_t			group;
	
	debugID = tracepoint.debugid;
	group = debugID & 0xFFFFFC00;

	if ( !gBasicFormatting )
	{
		switch ( group )
		{
			case USB_CONTROLLER_TRACE(0):
				CollectTraceController( tracepoint );		
				break;
				
			case USB_CONTROLLER_UC_TRACE(0):
				CollectTraceControllerUserClient(tracepoint);
				break;
				
			case USB_DEVICE_TRACE(0):
				CollectTraceDevice (tracepoint); //2,
				break;
				
			case USB_DEVICE_UC_TRACE(0):
				CollectTraceDeviceUserClient (tracepoint); //3,
				break;
				
			case USB_HUB_TRACE(0)				:
				CollectTraceHub (tracepoint); //4,
				break;				
				
			case USB_HUB_PORT_TRACE(0)		:
				CollectTraceHubPort (tracepoint); //5,
				break;
				
			case USB_HUB_UC_TRACE(0)			:
				CollectTraceHSHubUserClient (tracepoint); //6,
				break;
				
			case USB_HID_TRACE(0)				:
				CollectTraceHID	(tracepoint); //7,
				break;
				
			case USB_PIPE_TRACE(0)			:
				CollectTracePipe (tracepoint); //8,
				break;				
				
			case USB_INTERFACE_UC_TRACE(0) :
				CollectTraceInterfaceUserClient	(tracepoint); //9,
				break;

			case USB_ENUMERATION_TRACE(0) :
				CollectTraceEnumeration	(tracepoint); //9,
				break;
				
			case USB_UHCI_TRACE(0)			:
				CollectTraceUHCI (tracepoint); //11,
				break;
				
			case USB_UHCI_UIM_TRACE(0)		:
				CollectTraceUHCIUIM	(tracepoint); 
				break;
				
			case USB_UHCI_INTERRUPTS_TRACE(0):
				CollectTraceUHCIInterrupts(tracepoint); 
				break;
				
			case USB_OHCI_TRACE(0):
				CollectTraceOHCI	(tracepoint); 
				break;
		
			case USB_OHCI_INTERRUPTS_TRACE(0):	
				CollectTraceOHCIInterrupts	(tracepoint); 
				break;
				
			case USB_EHCI_TRACE(0):	
				CollectTraceEHCI	(tracepoint); 
				break;
				
			case USB_EHCI_HUBINFO_TRACE(0):	
				CollectTraceEHCIHubInfo	(tracepoint); 
				break;
				
			case USB_EHCI_INTERRUPTS_TRACE(0):	
				CollectTraceEHCIInterrupts	(tracepoint); 
				break;
				
			case USB_HUB_POLICYMAKER_TRACE(0):	
				CollectTraceHubPolicyMaker	(tracepoint); 
				break;
				
			case USB_COMPOSITE_DRIVER_TRACE(0):
				CollectTraceCompositeDriver	(tracepoint); 
				break;

			case USB_OUTSTANDING_IO_TRACE(0):
				CollectTraceOutstandingIO	(tracepoint); 
				break;

			case USB_AUDIO_DRIVER_TRACE(0):
				CollectTraceAudioDriver	(tracepoint); 
				break;
				
			default:   
				CollectTraceUnknown( tracepoint );
				break;
		}
	}
	else
	{
		CollectTraceBasic( tracepoint );
	}
}


#pragma mark Family Tracepoints

static void 
CollectTraceController( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		
		case USB_CONTROLLER_TRACE( kTPControllerStart ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Controller", "start", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "end", parg1, NULL );
				}
				else if ( arg4 == 2 )
					log(info, "Controller", "end", parg1, "error starting");
			} 
			else 
			{
				log(info, "Controller", "start", parg1, "error starting status = 0x%x", arg2 );
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllerControlPacketHandler ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Controller", "PacketHandler", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d status 0x%x bufferSizeRemaining %d", ((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), (uint32_t)arg3, (uint32_t)arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Controller", "PacketHandler", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d todo 0x%x status 0x%x", ((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), (uint32_t)arg3, (uint32_t)arg4 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "PacketHandler", parg1, "calling clear TT status = 0x%x controllerv2 = 0x%x", (uint32_t)arg2, (uint32_t)arg3 );
				}
				else
					log(info, "Controller", "PacketHandler", parg1, "status = 0x%x", (uint32_t)arg2 );
			}
			break;
		
		case USB_CONTROLLER_TRACE( kTPControllerMakeDevice ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Controller", "MakeDevice Start", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "MakeDevice End", parg1, "new device %x", (uint32_t)arg2 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "MakeDevice", parg1, "error getting address - releasing newDev");
				}
				else
					log(info, "Controller", "MakeDevice", parg1, "err=0x%x device=0x%x - releasing device %x", (uint32_t)arg2, (uint32_t)arg3, (uint32_t)arg4 );
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllerMakeHubDevice ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Controller", "MakeHubDevice Start", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "MakeHubDevice End", parg1, "new device %p", (void *)parg2 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "MakeHubDevice", parg1, "error getting address - releasing newDev");
				}
				else
					log(info, "Controller", "MakeHubDevice", parg1, "error=0x%x releasing device =0x%x - error setting address %x", (uint32_t)arg2, (uint32_t)arg3, (uint32_t)arg4 );
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllerCreateRootHubDevice ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Controller", "CreateRHDevice Start", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "CreateRHDevice End", parg1, "error = %x", (uint32_t)arg2 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "CreateRHDevice", parg1, "unable to get root hub descriptor");
				}
				else if ( arg4 == 2 )
				{
					log(info, "Controller", "CreateRHDevice", parg1, "unable to create and initialize root hub device %x", (uint32_t)arg2 );
				}
				else if ( arg4 == 3 )
				{
					log(info, "Controller", "CreateRHDevice", parg1, "bus %d already taken", (uint32_t)arg2 );
				}
				else
					log(info, "Controller", "CreateRHDevice", parg1, "controller does not support sleep, NOT setting characteristic in root hub (0x%x)", (uint32_t)arg2);
			}
			break;
	
		case USB_CONTROLLER_TRACE( kTPControllerDisjointCompletion ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "DisjointCompletion Start", parg1, "command 0x%x status 0x%x bufferSizeRemaining %d", (uint32_t)arg2, (uint32_t)arg3, (uint32_t)arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "DisjointCompletion End", parg1, "<- target parameter 0x%x  status 0x%x bufferSizeRemaining %d", (uint32_t)arg2, (uint32_t)arg3, (uint32_t)arg4 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "DisjointCompletion", parg1, "no dmaCommand");
				}
				else if ( arg4 == 2 )
					log(info, "Controller", "DisjointCompletion", parg1, "buf(%p) doesn't match getMemoryDescriptor(%p)", (void*)parg2, (void *)parg3 );
			}
			break;
		
		case USB_CONTROLLER_TRACE( kTPControllerCheckForDisjointDescriptor ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Check For DD Start", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Check For DD End", parg1, "status = 0x%x", arg2 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "Check For DD", parg1, "- no dmaCommand status 0x%x", arg2 );
				}
				else if ( arg4 == 2 )
				{
					log(info, "Controller", "Check For DD", parg1, "- mismatched memory descriptor (0x%x) and dmaCommand memory descriptor (0x%x)", arg2, arg3 );
				}
				else if ( arg4 == 3 )
				{
					log(info, "Controller", "Check For DD", 0, "offset (%d), length (%d), segLength (%d)", arg1, arg2, arg3 );
				}
				else if ( arg4 == 4 )
				{
					log(info, "Controller", "Check For DD", 0, "segLength (%d), total length (%d), numSegments (%d)", arg1, arg2, arg3 );
				}
				else if ( arg4 == 5 )
				{
					log(info, "Controller", "Check For DD", parg1, "could not allocate new buffer status = 0x%x", arg2 );
				}
				else if ( arg4 == 6 )
				{
					log(info, "Controller", "Check For DD", parg1, "bad copy on a write");
				}
				else if ( arg4 == 7 )
				{
					log(info, "Controller", "Check For DD", parg1, "error %x in prepare", arg2 );
				}
				else if ( arg4 == 8 )
				{
					log(info, "Controller", "Check For DD", parg1, "error %x in setMemoryDescriptor", arg2 );
				}
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllerClearTTHandler ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "ClearTTHandler Start", parg1, "command 0x%x status 0x%x bufferSizeRemaining %d", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "ClearTTHandler End", parg1, NULL );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "ClearTTHandler", parg1, "DMA Command Memory Descriptor (0x%x) does not match Request MemoryDescriptor (0x%x)", arg2, arg3 );
				}
				else if ( arg4 == 2 )
				{
					log(info, "Controller", "ClearTTHandler", parg1, "dmaCommand (0x%x) already cleared", arg2);
				}
				else if ( arg4 == 3 )
				{
					log(info, "Controller", "ClearTTHandler", parg1, "completing and freeing memory descriptor (0x%x)", arg2 );
				}
				else if ( arg4 == 4 )
				{
					log(info, "Controller", "ClearTTHandler", parg1, "missing memory descriptor");
				}
				else if ( arg4 == 5 )
					log(info, "Controller", "ClearTTHandler", parg1, "error %x response from hub, clearing hub endpoint stall", arg2 );
			}
			break;
		
		case USB_CONTROLLER_TRACE( kTPControllerClearTT ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "ClearTT Start", parg1, "function %x endpoint %d direction %d", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "ClearTT End", parg1, NULL );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "ClearTT", parg1, "high speed device, returning");
				}
				else if ( arg4 == 2 )
				{
					log(info, "Controller", "ClearTT", parg1, "Could not get a memory descriptor");
				}
				else if ( arg4 == 3 )
				{
					log(info, "Controller", "ClearTT", parg1, "Could not get a IOUSBCommand");
				}
				else if ( arg4 == 4 )
				{
					log(info, "Controller", "ClearTT", parg1, "No dmaCommand in the usb command");
				}
				else if ( arg4 == 5 )
				{
					log(info, "Controller", "ClearTT", parg1, "Could not get a IOUSBDevRequest");
				}
				else if ( arg4 == 6 )
					log(info, "Controller", "ClearTT", parg1, "error %x returned from ControlTransaction", arg2 );
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllerDoCreateEP ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "DoCreateEP Start", parg1, "hub %d port %d transferType %d", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "DoCreateEP End", parg1, "error 0x%x", arg2 );
			} 
			else 
			{
				log(info, "Controller", "DoCreateEP", parg1, "The USB 2.0 spec only allows Isoch EP with bInterval values of 1 through 16 error 0x%x interval %d", arg2, arg3 );
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllerReadV2 ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "ReadV2 Start", parg1, "address 0x%x direction %d reqcount %d", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "ReadV2 End", parg1, "error 0x%x", arg2 );
			} 
			else 
			{
				log(info, "Controller", "ReadV2", parg1, "SYNC xfer or immediate error with Disjoint Completion error 0x%x", arg2 );
			}
			break;
		

		case USB_CONTROLLER_TRACE( kTPControllerReturnIsochDoneQueue ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Return Isoc Done Q Start", parg1, "pEP 0x%x pTD 0x%x", arg2, arg3 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Return Isoc Done Q End", parg1, NULL );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "Return Isoc Done Q", 0, "kIOReturnBufferUnderrunErr (PCI issue perhaps)  Bus: %x, Address: %d, Endpoint: %d", arg1, arg2, arg3 );
				}
				else if ( arg4 == 2 )
				{
					log(info, "Controller", "Return Isoc Done Q", 0, "kIOReturnBufferOverrunErr (PCI issue perhaps)  Bus: %x, Address: %d, Endpoint: %d", arg1, arg2, arg3 );
				}
				else if ( arg4 == 3 )
				{
					log(info, "Controller", "Return Isoc Done Q", 0, "kIOReturnOverrun on IN - device babbling?  Bus: %x, Address: %d, Endpoint: %d", arg1, arg2, arg3 );
				}
				else if ( arg4 == 4 )
				{
					log(info, "Controller", "Return Isoc Done Q", 0, "_activeIsochTransfers went negative We lost one somewhere  Bus: %x, Address: %d, Endpoint: %d", arg1, arg2, arg3 ); 
				}
				else if ( arg4 == 5 )
				{
					log(info, "Controller", "Return Isoc Done Q", 0, " Bus: 0x%x, Address: %d, Endpoint: %d, status: 0x%x, calling handler %p", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg3, (void*)parg2 );
				}
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllersetPowerState ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "SetPowerState start", parg1, "powerStateOrdinal %d whatDevice 0x%x myPowerState %d", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "SetPowerState End", parg1,  "myPowerState %d", arg2 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "SetPowerState", 0, "whatDevice %p != this", (void *)parg1 );
				}
				else if ( arg4 == 2 )
				{
					log(info, "Controller", "SetPowerState", parg1, "isInactive - no op");
				}
				else if ( arg4 == 3 )
					log(info, "Controller", "SetPowerState", parg1, "powerStateOrdinal %d", arg2 );
			}
			break;

		case USB_CONTROLLER_TRACE( kTPControllerCheckPowerModeBeforeGatedCall ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "CheckPowerModeBeforeGatedCall Start", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "CheckPowerModeBeforeGatedCall End", parg1, "status 0x%x", arg2 );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "Controller", "CheckPowerModeBeforeGatedCall", parg1, "myPowerState %d onThread is true while !_controllerAvailable status 0x%x", arg2, arg3 );
				}
				else if ( arg4 == 2 )
					log(info, "Controller", "CheckPowerModeBeforeGatedCall", parg1, "myPowerState %d status 0x%x", arg2, arg3 );
			}
			break;
			
		case USB_CONTROLLER_TRACE( kTPControllerGatedPowerChange ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "GatedPowerChange Start", parg1, "powerStateOrdinal %d OldState %d", arg2, arg3 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "GatedPowerChange End", parg1, NULL );
			} 
			else 
			{
				log(info, "Controller", "GatedPowerChange", parg1, "Could not create root hub device upon wakeup - error 0x%x!", arg2 );
			}
			break;


		case USB_CONTROLLER_TRACE( kTPControllerCheckForRootHubChanges ):
			log(info, "Controller", "CheckForRootHubChanges", parg1, "_rootHubStatusChangedBitmap(0x%x) with no _rootHubDevice or policy maker!!", arg2 );
			break;

		case USB_CONTROLLER_TRACE( kTPControllerRootHubQueueInterruptRead ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "RHQInterruptRead Start", parg1, "buffer %x bufferlength %d", arg2, arg3 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "RHQInterruptRead End", parg1, "error 0x%x bufferlength %d", arg2, arg3 );
			} 
			else 
			{
				log(info, "Controller", "RHQInterruptRead", parg1, "this is index(%d) - UNEXPECTED?", arg2 );
			}
			break;
		
		case USB_CONTROLLER_TRACE( kTPControllerRootHubTimer ):
			if ( arg4 == 4 )
			{
				if (gPrintRHTimerFired)
					log(info, "Controller", "RootHubTimerFired", parg1, "PolicyMaker[%p] powerState[%d]", (void*) parg2, arg3 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "Controller", "CheckForRootHubChanges", parg1, "stopping timer and calling complete");
			}
			else if ( arg4 == 2 )
			{
				log(info, "Controller", "RootHubStopTimer", parg1, NULL);
			}
			else if ( arg4 == 3 )
			{
				log(info, "Controller", "RootHubAbortInterruptRead", parg1, NULL);
			}
			else if ( arg4 == 1 )
			{
				log(info, "Controller", "RootHubStartTimer32", parg1, "Polling Rate: %d", arg2);
			}
			else if ( arg4 == 6 )
			{
				log(info, "Controller", "RootHubStartTimer32", parg1, "invalid polling rate of %d", arg2 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "Controller", "RootHubAbortInterruptRead", parg1, "No timer!!!");
			}
			
			break;
		
		case USB_CONTROLLER_TRACE( kTPControllerV3Start ):
			if ( arg4 == 1 )
			{
				log(info, "Controller", "V3Start", parg1, "CheckForEHCIController returned status 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Controller", "V3Start", parg1, "couldn't allocate timer event source - status 0x%x", arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Controller", "V3Start", parg1, "couldn't add timer event source - status 0x%x", arg2 );
			}
			else if ( arg4 == 4 )
				log(info, "Controller", "V3Start", parg1, "InitForPM returned status 0x%x", arg2 );
			break;
		
		case USB_CONTROLLER_TRACE( kTPAllocatePowerStateArray ):
			log(info, "Controller", "AllocatePowerStateArray", parg1, "no memory 0x%x", arg2 );
			break;

		case USB_CONTROLLER_TRACE( kTPInitForPM ):
			log(info, "Controller", "InitForPM", parg1, "status 0x%x", arg2 );
			break;

		case USB_CONTROLLER_TRACE( kTPIsocIOLL ):
			if ( arg4 == 1 )
			{
				log(info, "Controller", "IsocIOLL", parg1, "no IODMACommand in the IOUSBCommand status 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Controller", "IsocIOLL", parg1, "status 0x%x", arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Controller", "IsocIOLL", parg1, "No completion. Returning status 0x%x", arg2 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Controller", "IsocIOLL", parg1, "Could not get _commandGate.  Returning status 0x%x", arg2 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "Controller", "IsocIOLL", parg1, "Could not get a IOUSBIsocCommand status 0x%x", arg2 );
			}
			else if ( arg4 == 6 )
				log(info, "Controller", "IsocIOLL", parg1, "Direction is not kUSBOut or kUSBIn (%d).  Returning status 0x%x", arg2, arg3 );
			break;
		
		case USB_CONTROLLER_TRACE( kTPIsocIO ):
			if ( arg4 == 1 )
			{
				log(info, "Controller", "Isoc IO", parg1, "no IODMACommand in the IOUSBCommand status 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
				log(info, "Controller", "Isoc IO", parg1, "dmaCommand->setMemoryDescriptor failed with status 0x%x", arg2 );
			break;
			
		case USB_CONTROLLER_TRACE( kTPControllerRead ):
			if ( arg4 == 1 )
			{
				log(info, "Controller", "Read", parg1, "Could not get _commandGate.  Returning status 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Controller", "Read", parg1, "no DMA COMMAND status 0x%x", arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Controller", "Read", parg1, "dmaCommand (0x%x) already contains memory descriptor (0x%x) - clearing", arg2, arg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Controller", "Read", parg1, "error 0x%x preparing memory descriptor (0x%x)", arg2, arg3 );
			}
			else if ( arg4 == 5 )
				log(info, "Controller", "Read", parg1, "error 0x%x attempting to set the memory descriptor to the dmaCommand", arg2 );
			break;


		case USB_CONTROLLER_TRACE( kTPControllerWrite ):
 			if ( arg4 == 1 )
			{
				log(info, "Controller", "Write", parg1, "no DMA COMMAND status 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Controller", "Write", parg1, "dmaCommand (0x%x) already contains memory descriptor (0x%x) - clearing", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Controller", "Write", parg1, "error 0x%x preparing buffer (0x%x)", arg2, arg3 );
			}
			else if ( arg4 == 4 )
				log(info, "Controller", "Write", parg1, "error 0x%x attempting to set the memory descriptor to the dmaCommand", arg2 );
			break;
		
		case USB_CONTROLLER_TRACE( kTPCompletionCall ):
 			if ( arg4 == 1 )
			{
				log(info, "Controller", "IsocCompletion", parg1, "completion: %p, status:  0x%x", (void *)parg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Controller", "Complete", parg1, "completion: %p, status:  0x%x", (void *)parg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Controller", "CompleteTS", parg1, "completion: %p, status:  0x%x", (void *)parg2, arg3 );
			}
			else
			{
				log(info, "Controller", "Unknown", parg1, NULL);
			}
			break;
			
		case USB_CONTROLLER_TRACE( kTPControlTransaction ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Control Start", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s)  bmRequestType: 0x%2.02x, bRequest: 0x%2.02x, wValue: 0x%4.04x, wIndex: 0x%4.04x, wLength: 0x%4.04x", 
									((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", 
									((arg3 >> 24) & 0xFF), ((arg3 >> 16) & 0xFF), (arg3 & 0xFFFF), 
									((arg4 >> 16) & 0xFFFF), (arg4 & 0xFFFF)
					);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Control End", parg1, "error 0x%x", arg2 );
			} 
			else 
			{
#ifdef __LP64__
				log(info, "Controller", "ControlTxction", 0, "Bus: 0x%x, Address: %d, Endpoint: %d, (out) length: %d, data: 0x%16.016qx 0x%16.016qx", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt64(arg3), OSSwapInt64(arg4) );
#else
				log(info, "Controller", "ControlTxction", 0, "Bus: 0x%x, Address: %d, Endpoint: %d  (out) length: %d, data: 0x%8.08x 0x%8.08x", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt32(arg3), OSSwapInt32(arg4) );
#endif
			}
				break;
			
		case USB_CONTROLLER_TRACE( kTPInterruptTransaction ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Interrupt Start", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s)  reqCount: %d", 
					((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", 
					arg3
					);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Interrupt End", parg1, "error 0x%x, (completion timeout: %d, noData timeout: %d)", arg2, arg3, arg4 );
			} 
			else 
			{
#ifdef __LP64__
				log(info, "Controller", "Interrupt Start Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d, (out)  length: %d, data: 0x%16.016qx 0x%16.016qx", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt64(arg3), OSSwapInt64(arg4) );
#else
				log(info, "Controller", "Interrupt Start Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d  (out)  length: %d, data: 0x%8.08x 0x%8.08x", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt32(arg3), OSSwapInt32(arg4) );
#endif
			}

			break;
			
			
		case USB_CONTROLLER_TRACE( kTPBulkTransaction ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Bulk Start", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s)  reqCount: %d", 
					((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", 
					arg3
					);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Bulk End", parg1, "error 0x%x, (completion timeout: %d, noData timeout: %d)", arg2, arg3, arg4 );
			} 
			break;
			
		case USB_CONTROLLER_TRACE( kTPIsocTransaction ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Isoc Start", parg1, "%sBus: 0x%x, Address: %d, Endpoint: %d, (%s)  FrameStart: %d NumFrames: %d", 
					( arg2 & 0x80000000) ? "Low Latency " : "", ((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0x7F) == kUSBIn ? "in" : "out", 
					arg3, arg4
					);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Isoc End", parg1, "error 0x%x   frameListPtr: 0x%x", arg2, arg3);
			} 
			break;
			
		case USB_CONTROLLER_TRACE( kTPInterruptPacketHandlerData ):
#ifdef __LP64__
			log(info, "Controller", "Interrupt Complete Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d, (in)  length: %d, data: 0x%16.016qx 0x%16.016qx", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt64(arg3), OSSwapInt64(arg4) );
#else
			log(info, "Controller", "Interrupt Complete Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d  (in)  length: %d, data: 0x%8.08x 0x%8.08x", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt32(arg3), OSSwapInt32(arg4) );
#endif
			break;
			
		case USB_CONTROLLER_TRACE( kTPBulkPacketHandlerData ):
#ifdef __LP64__
			log(info, "Controller", "Bulk Complete Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d, (in)  length: %d, data: 0x%16.016qx 0x%16.016qx", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt64(arg3), OSSwapInt64(arg4) );
#else
			log(info, "Controller", "Bulk Complete Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d  (in)  length: %d, data: 0x%8.08x 0x%8.08x", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt32(arg3), OSSwapInt32(arg4) );
#endif
			break;
			
		case USB_CONTROLLER_TRACE( kTPBulkTransactionData ):
#ifdef __LP64__
			log(info, "Controller", "Bulk Start Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d, (out)  length: %d, data: 0x%16.016qx 0x%16.016qx", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt64(arg3), OSSwapInt64(arg4) );
#else
			log(info, "Controller", "Bulk Start Data", 0, "Bus: 0x%x, Address: %d, Endpoint: %d  (out)  length: %d, data: 0x%8.08x 0x%8.08x", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt32(arg3), OSSwapInt32(arg4) );
#endif
			break;
			
		case USB_CONTROLLER_TRACE( kTPControlPacketHandlerData ):
#ifdef __LP64__
			log(info, "Controller", "Packet Handler", 0, "Bus: 0x%x, Address: %d, Endpoint: %d, (out)  length: %d, data: 0x%16.016qx 0x%16.016qx", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt64(arg3), OSSwapInt64(arg4) );
#else
			log(info, "Controller", "Packet Handler", 0, "Bus: 0x%x, Address: %d, Endpoint: %d  (out)  length: %d, data: 0x%8.08x 0x%8.08x", ((arg1 >> 16) & 0xFF), ((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), arg2, OSSwapInt32(arg3), OSSwapInt32(arg4) );
#endif
			break;
			
		case USB_CONTROLLER_TRACE( kTPDevZeroLock ):
			if (arg4 == 0)
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "_devZeroLock available without commandSleep");
			}
			else if ( arg4 == 1 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "_devZeroLock held by someone else - calling commandSleep to wait for lock");
			}
			else if ( arg4 == 2 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "commandSleep woke up normally (THREAD_AWAKENED) _devZeroLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 3 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "commandSleep interrupted (THREAD_INTERRUPTED) _devZeroLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 4 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "commandSleep restarted (THREAD_RESTART) _devZeroLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 5 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "woke up with status (kIOReturnNotPermitted) - we do not hold the WL!");
			}
			else if ( arg4 == 6 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "woke up with unknown status 0x%x", arg2);
			}
			else if ( arg4 == 7 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "commandSleep timeout out (THREAD_TIMED_OUT) _devZeroLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 8 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "setting _devZeroLock to false and calling commandWakeup");
			}
			else if ( arg4 == 9 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "setting _devZeroLock to true");
			}
			else if ( arg4 == 10 )
			{
				log(info, "Controller", "ProtectedDevZeroLock", parg1, "setting _devZeroLock to true because we woke up with an error from commandSleep()'d, but the lock was false");
			}
			break;
			
		case USB_CONTROLLER_TRACE( kTPDoIOTransferBulkSync ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Bulk Sync Start", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s) comp timeout: %d, noData timeout: %d  about to commandSleep()", 
					((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", arg3, arg4
					);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Bulk Sync End", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s)  commandWake: 0x%x, status: 0x%x", 
					((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", arg3, arg4
					);
			} 
			break;
		case USB_CONTROLLER_TRACE( kTPDoIOTransferIntrSync ):
			if ( qualifier == DBG_FUNC_START ) 
			{
				log(info, "Controller", "Intr Sync Start", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s) comp timeout: %d, noData timeout: %d  about to commandSleep()", 
					((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", arg3, arg4
					);
			}
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "Controller", "Intr Sync End", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s)  commandWake: 0x%x, status: 0x%x", 
					((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", arg3, arg4
					);
			}

		case USB_CONTROLLER_TRACE( kTPBulkPacketHandler ):
			log(info, "Controller", "Bulk Complete", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s)  actual: %d, status: 0x%x", 
				((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", 
				arg3, arg4
				);
			break;
			
		case USB_CONTROLLER_TRACE( kTPInterruptPacketHandler ):
			log(info, "Controller", "Interrupt Complete", parg1, "Bus: 0x%x, Address: %d, Endpoint: %d, (%s)  actual: %d, status: 0x%x", 
				((arg2 >> 16) & 0xFF), ((arg2 >> 8) & 0xFF), ((arg2 >> 0) & 0xFF), ((arg2 >> 24) & 0xFF) == kUSBIn ? "in" : "out", 
				arg3, arg4
				);
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceControllerUserClient( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_CONTROLLER_UC_TRACE( kTPControllerUCStart ):
			log(info, "Controller", "UC Start", parg1, "fOwner 0x%x status 0x%x", arg2, arg3 );
			break;

		case USB_CONTROLLER_UC_TRACE( kTPControllerUCOpen ):
			log(info, "Controller", "UC Open", parg1, "open fOwner 0x%x options %x status 0x%x", arg2, arg3, arg4 );
			break;
		
		case USB_CONTROLLER_UC_TRACE( kTPControllerUCReadRegister ):
			if ( arg2 == kIOReturnBadArgument )
			{
				log(info, "Controller", "UC ReadRegister", parg1, "fOwner 0x%x status 0x%x", arg2, arg3 );
			}
			else
				log(info, "Controller", "UC ReadRegister", parg1, "Offset 0x%x Size %d", arg2, arg3 );
			break;
		
		case USB_CONTROLLER_UC_TRACE( kTPControllerUCWriteRegister ):
			if ( parg1 == 1 )
			{
				log(info, "Controller", "UC WriteRegister", parg1, "Value 0x%x", arg2 );
			}
			else
				log(info, "Controller", "UC WriteRegister", parg1, "fOwner 0x%x Offset 0x%x Size %d", arg2, arg3, arg4 );
			break;
		
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceDevice ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_DEVICE_TRACE( kTPDeviceInit ):
			log(info, "Device", "Init", parg1, "<- deviceAddress Error allocating getConfigLock powerAvailable %d speed %d maxpacketsize %d", arg2, arg3, arg4 );
			break;
		
		case USB_DEVICE_TRACE( kTPDeviceResetDevice ):
			if ( arg4 == 0 )
			{
				log(info, "Device", "ResetDevice", parg1, "port %d, called on workloop thread !, returning kIOUSBSyncRequestOnWLThread ", arg2 );
			}
			else if ( arg4 == 1 )
			{
				log(info, "Device", "ResetDevice", parg1, "port %d, called while inactive, returning kIOReturnNoDevice ", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "ResetDevice", parg1, "port %d, called while Reset or Renumerate in progress, returning kIOReturnNotPermitted ", arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Device", "ResetDevice", parg1, "port %d, sending message 0x%x to clients", arg2, arg3);
			}
			else if ( arg4 == 4 )
			{
				log(info, "Device", "ResetDevice", parg1, "port %d, _DO_MESSAGE_CLIENTS_THREAD already queued", arg2);
			}
			else if ( arg4 == 5 )
			{
				log(info, "Device", "ResetDevice", parg1, "port %d, returning 0x%x", arg2, arg3);
			}
			break;
		
		case USB_DEVICE_TRACE( kTPDeviceGetFullConfigurationDescriptor ):
			if ( arg4 == 1 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "VID 0x%x PID 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "error 0x%x getting first %d bytes of config descriptor", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "unable to get memory buffer (capacity requested: %d)", arg2 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "error 0x%x getting full %d bytes of config descriptor", arg2, arg3 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "(index %d) - called on workloop thread, returning NULL", arg2 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "(index %d) - found cached configDescriptor %p", arg2, (void *)parg3 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "(index %d) - TakeGetConfigLock returned error 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 8 )
			{
				log(info, "Device", "GetFullConfig Desc", parg1, "(index %d) - returning configDescriptor %p", arg2, (void*)parg3 );
			}
			else
				log(info, "Device", "GetFullConfig Desc", 0, "Overrun error and data returned is not correct (%d, %d, %d) status 0x%x", arg1, arg2, arg3, arg4 );
			break;

		case USB_DEVICE_TRACE( kTPDeviceMessage ):
			if ( arg4 == 1 )
			{
				log(info, "Device", "Message", parg1, "message 0x%x 1 cannot call commandGate->wakeup because there is no gate", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "Message", parg1, "message 0x%x DANGER could not recreate pipe Zero after reset status 0x%x", arg2, arg3 );
			}
			break;
		
		case USB_DEVICE_TRACE( kTPDeviceGetDeviceDescriptor ):
			log(info, "Device", "Get Desc", parg1, "error 0x%x getting device device descriptor bmRequestType 0x%x", arg2, arg3);
			break;

		case USB_DEVICE_TRACE( kTPDeviceSetConfiguration ):
			if ( arg4 == 1 )
			{
				log(info, "Device", "Set Config", parg1, "not enough bus power to configure device want %d, available: %d", arg2, arg3);
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "Set Config", parg1, "setting config error 0x%x device descriptor bmRequestType 0x%x wValue 0x%x", arg2, arg3, arg4 );
			}
			break;

		case USB_DEVICE_TRACE( kTPDeviceSetFeature ):
			log(info, "Device", "Set feature", parg1, "setting feature bmRequestType %d wValue %d error 0x%x", arg3, arg4, arg2 );
			break;

		case USB_DEVICE_TRACE( kTPDeviceDeviceRequest ):
			if ( arg4 == 1 )
			{
				log(info, "Device", "Request", parg1, "1 while terminating status 0x%x wValue %d", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "Request", parg1, "2 while terminating status 0x%x wValue %d", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Device", "Request", parg1, "3 while terminating status 0x%x wValue %d", arg2, arg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Device", "Request", parg1, "4 while terminating status 0x%x wValue %d", arg2, arg3 );
			}
			break;


		case USB_DEVICE_TRACE( kTPDeviceGetConfiguration ):
			log(info, "Device", "Get Config", parg1, "error 0x%x getting config bmRequestType 0x%x", arg2, arg3 );
			break;

		case USB_DEVICE_TRACE( kTPDeviceGetDeviceStatus ):
			log(info, "Device", "Get DeviceStatus", parg1, "error 0x%x getting device status bmRequestType 0x%x", arg2, arg3 );
			break;

		case USB_DEVICE_TRACE( kTPDeviceSuspendDevice ):
			if ( arg4 == 1 )
			{
				log(info, "Device", "Suspend Device", parg1, "port %d, called on workloop thread ", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "Suspend Device", parg1, "port %d, called while Reset or Renumerate in progress, returning kIOReturnNotPermitted ", arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Device", "Suspend Device", parg1, "port %d, called while inactive, returning kIOReturnNoDevice ", arg2 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Device", "Suspend Device", parg1, "Device at: 0x%x, %s", arg2, arg3 ? "suspend" : "resume");
			}
			else if ( arg4 == 5 )
			{
				log(info, "Device", "Suspend Device", parg1, "port %d, returning 0x%x", arg2, arg3);
			}
			else if ( arg4 == 6 )
			{
				log(info, "Device", "Suspend Device", parg1, "port %d, sending message 0x%x to clients", arg2, arg3);
			}
			else if ( arg4 == 7 )
			{
				log(info, "Device", "Suspend Device", parg1, "port %d, _DO_MESSAGE_CLIENTS_THREAD already queued", arg2);
			}
			break;

		case USB_DEVICE_TRACE( kTPDeviceReEnumerateDevice ):
			if ( arg4 == 0 )
			{
				log(info, "Device", "ReEnumerateDevice", parg1, "port %d, called on workloop thread !, returning kIOUSBSyncRequestOnWLThread ", arg2 );
			}
			else if ( arg4 == 1 )
			{
				log(info, "Device", "ReEnumerateDevice", parg1, "port %d, called while inactive, returning kIOReturnNoDevice ", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "ReEnumerateDevice", parg1, "port %d, called while Reset or Renumerate in progress, returning kIOReturnNotPermitted ", arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Device", "ReEnumerateDevice", parg1, "port %d setting extra reset time options! options 0x%x", arg2, arg3);
			}
			else if ( arg4 == 4 )
			{
				log(info, "Device", "ReEnumerateDevice", parg1, "port %d options 0x%x", arg2, arg3);
			}
			else if ( arg4 == 5 )
			{
				log(info, "Device", "ReEnumerateDevice", parg1, "port %d returning 0x%x", arg2, arg3);
			}
			break;
			
		case USB_DEVICE_TRACE( kTPDeviceConfigLock ):
			if ( arg4 == 1 )
			{
				log(info, "Device", "TakeGetConfigLock", parg1, "no workloop(%p) or no gate(%p)!", (void*)parg2, (void*)parg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Device", "TakeGetConfigLock", parg1, "called onThread -- not allowed to do so" );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Device", "ReleaseGetConfigLock", parg1, "no workloop(%p) or no gate(%p)!", (void*)parg2, (void*)parg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "_GETCONFIGLOCK held by someone else - calling commandSleep to wait for lock");
			}
			else if ( arg4 == 5 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "commandSleep woke up normally (THREAD_AWAKENED) _GETCONFIGLOCK(%s)",arg2 ? "true" : "false");
			}
			else if ( arg4 == 6 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "commandSleep timeout out (THREAD_TIMED_OUT) _GETCONFIGLOCK(%s)",arg2 ? "true" : "false");
			}
			else if ( arg4 == 7 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "commandSleep interrupted (THREAD_INTERRUPTED) _GETCONFIGLOCK(%s)",arg2 ? "true" : "false");
			}
			else if ( arg4 == 8 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "commandSleep restarted (THREAD_RESTART) _GETCONFIGLOCK(%s)",arg2 ? "true" : "false");
			}
			else if ( arg4 == 9 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "woke up with status (kIOReturnNotPermitted) - we do not hold the WL");
			}
			else if ( arg4 == 10 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "woke up with unknown status 0x%x", arg2);
			}
			else if ( arg4 == 11 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "setting _GETCONFIGLOCK to false and calling commandWakeup");
			}
			else if ( arg4 == 12 )
			{
				log(info, "Device", "FindConfig", parg1, "config %d, returning %p", arg2, (void*)parg3);
			}
			else if ( arg4 == 13 )
			{
				log(info, "Device", "ChangeGetConfigLock", parg1, "setting _GETCONFIGLOCK to true");
			}
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceDeviceUserClient ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_DEVICE_UC_TRACE( kTPDeviceUCChangeOutstandingIO ):
			if ( arg4 == 1 )
			{
				log(info, "DeviceUserClient", "ChangeOutstandingIO", parg1, "invalid target direction %d status 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "DeviceUserClient", "ChangeOutstandingIO", parg1, "invalid target direction %d", arg2 );
			}
			break;
		
		case USB_DEVICE_UC_TRACE( kTPDeviceUCGetGatedOutstandingIO ):
			log(info, "DeviceUserClient", "GetGatedOutstandingIO", parg1, "invalid target status 0x%x", arg2 );
			break;
		
		case USB_DEVICE_UC_TRACE( kTPDeviceUCDeviceRequestIn ):
			if ( arg4 == kIOReturnBadArgument )
			{
				log(info, "DeviceUserClient", "RequestIn", arg4, "had a NULL completion bmRequestType 0x%x bRequest 0x%x wValue 0x%x ", arg1, arg2, arg3 );
			}
			else if ( arg4 == kIOReturnNoMemory )
			{
				log(info, "DeviceUserClient", "RequestIn", parg1, "IOMemoryDescriptor::withAddressRange returned NULL invalid target buffer 0x%x size %d", arg2, arg3 );
			}
			break;
		

		case USB_DEVICE_UC_TRACE( kTPDeviceUCDeviceRequestOut ):
			if ( arg4 == kIOReturnBadArgument )
			{
				log(info, "DeviceUserClient", "RequestOut", arg4, "had a NULL completion bmRequestType 0x%x bRequest 0x%x wValue 0x%x ", arg1, arg2, arg3 );
			}
			else if ( arg4 == kIOReturnNoMemory )
			{
				log(info, "DeviceUserClient", "RequestOut", arg4, "0x%x IOMemoryDescriptor::withAddressRange returned NULL invalid target buffer 0x%x size %d", arg1, arg2, arg3 );
			}
			break;
			
		case USB_DEVICE_UC_TRACE( kTPDeviceUCReqComplete ):
			log(info, "DeviceUserClient", "ReqComplete", parg1, "Error: 0x%8.08x, bytesTransferred: %d of %d", arg2, arg4 - arg3, arg4 );
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}

static void 
CollectTraceHub ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_HUB_TRACE( kTPHubStart ):
			log(info, "Hub", "start", parg1, "USB Generic Hub @ address 0x%x location 0x%x ", arg2, arg3 );
			break;
		
		case USB_HUB_TRACE( kTPHubMessage ):
			log(info, "Hub", "message", parg1, "(Reset) USB Generic Hub @ 0x%x location 0x%x ", arg2, arg3 );
			break;

		case USB_HUB_TRACE( kTPHubWillTerminate ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "WillTerminate", parg1, "interruptPipe->Abort returned provider 0x%x error 0x%x ", arg2, arg3 );
			}
			else
				log(info, "Hub", "WillTerminate", parg1, "port %d had the dev zero lock portIndex 0x%x devZero 0x%x ", arg2, arg3, arg4 );
			break;


		case USB_HUB_TRACE( kTPHubPowerStateWillChangeTo ):
			log(info, "Hub", "powerStateWillChangeTo", parg1, "an init thread or status changed thread is still active for some port - waiting 100ms (retries=%d) _powerStateChangingTo 0x%x kIOUSBHubPowerStateLowPower ", arg2, arg3);
			break;

		case USB_HUB_TRACE( kTPHubPowerChangeDone ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "powerChangeDone", parg1, "ResetDevice returned 0x%x", arg2 );
			}
			else if ( arg4 ==  4 ) // kIOUSBHubPowerStateOn
			{
				log(info, "Hub", "powerChangeDone", parg1, "calling changePowerStateToPriv(kIOUSBHubPowerStateOn) ");
			}
			else
				log(info, "Hub", "powerChangeDone", parg1, "calling ResetDevice() fromState %d myPowerState %d kIOUSBHubPowerStateOff", arg2, arg3 );
			break;

		case USB_HUB_TRACE( kTPHubConfigureHub ):
			log(info, "Hub", "Configure", parg1, "there are no ports on this hub numPorts %d ", arg2 );
			break;

		case USB_HUB_TRACE( kTPHubCheckPowerRequirements ):
			log(info, "Hub", "Check Power", parg1, "insufficient power to turn on ports 0x%x ", arg2 );
			break;

		case USB_HUB_TRACE( kTPHubHubPowerChange ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Power Change", parg1, "kIOUSBHubPowerStateLowPower hub going to doze - but this hub does NOT allow it _portSuspended %d _myPowerState %d  ", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Power Change", parg1, "kIOUSBHubPowerStateLowPower HubPowerChange ((hub @ 0x%x) - hub going to doze - my port is suspended! UNEXPECTED", arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Hub", "Power Change", parg1, "SuspendDevice returned status 0x%x", arg2 );
			}
			else if ( arg4 == 4 )
			{
			log(info, "Hub", "Power Change", parg1, "hub is dead, so just acknowledge the HubPowerChange");
			}
			else if ( arg4 == 5 )
			{
				log(info, "Hub", "Power Change", parg1, "hub doesn't support sleep, don't do anything on Suspend Characteristics 0x%x", arg2 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "Hub", "Power Change", parg1, "SuspendDevice returned error 0x%x", arg2 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "Hub", "Power Change", parg1, "_waitForPortResumesThread already queued");
			}
			else
				log(info, "Hub", "Power Change", 0, "kIOUSBHubPowerStateLowPower Hub @ 0x%x port %d status: 0x%x change: 0x%x connected, enabled and not suspended - UNEXPECTED", arg1, arg2, arg3, arg4 );
			break;

		case USB_HUB_TRACE( kTPHubAreAllPortsDisconnectedOrSuspended ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "AllPortsDisconnectedOrSuspended", parg1, "the _ports went away - bailing portNum %d _hubDescriptor.numPorts %d  ", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "AllPortsDisconnectedOrSuspended", 0, "_locationID %d  port %d is not powered on! portPMState=%d", arg1, arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Hub", "AllPortsDisconnectedOrSuspended", 0, "GetPortStatus _locationID %d for port %d returned 0x%x", arg1, arg2, arg3 );
			}
			break;

		case USB_HUB_TRACE( kTPHubSuspendPorts ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Suspend Ports", parg1, "error 0x%x suspending port %d  ", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Suspend Ports", parg1, "error 0x%x - resetting my port", arg2 );
			}
			break;
		
		case USB_HUB_TRACE( kTPHubSetPortFeature ):
			log(info, "Hub", "Set PortFeature", parg1, "feature %d to port %d got error (%x) from DoDeviceRequest  ", arg2, arg3, arg4 );
			break;

		case USB_HUB_TRACE( kTPHubClearPortFeature ):
			log(info, "Hub", "Clear PortFeature", parg1, "feature %d to port %d got error (%x) from DoDeviceRequest  ", arg2, arg3, arg4 );
			break;

		case USB_HUB_TRACE( kTPHubDoPortAction ):
			if ( arg2 == kIOReturnNoDevice )
			{
				log(info, "Hub", "DoPortAction", parg1, "_ports is NULL! - calling LowerPowerState and returning status 0x%x  ", arg2 );
			}
			else
				log(info, "Hub", "DoPortAction", parg1, "_checkForActivePortsThread already queued kIOUSBMessageHubSuspendPort ");
			break;
		
		case USB_HUB_TRACE( kTPHubInterruptReadHandler ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "InterruptReadHandler", parg1, "avoiding NULL _ports - unlike before!! 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "InterruptReadHandler", parg1, "status: 0x%x, bufferSizeRemaining: %d", arg2, arg3);
			}
			break;
			
		case USB_HUB_TRACE( kTPHubResetPortZero ):
			log(info, "Hub", "Reset Port Zero", parg1, "port %d - Releasing devZero lock", arg2 );
			break;

		case USB_HUB_TRACE( kTPHubProcessStateChanged ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Process State Changed", parg1, "in inner loop - we seem to have gone away. bailing - hubStatusSuccess 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Process State Changed", parg1, "hubStatusSuccess(FALSE) - this is weird");
			}
			else 
				log(info, "Hub", "Process State Changed", parg1, "portNum %d _myPowerState %d _powerStateChangingTo %d", arg2, arg3, arg4 );
			break;
		
		case USB_HUB_TRACE( kTPHubRearmInterruptRead ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Rearm InterruptRead", parg1, "_myPowerState %d state 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Rearm InterruptRead", parg1, "avoiding NULL _ports - unlike before!!");
			}
			else 
				log(info, "Hub", "Rearm InterruptRead", parg1, "error %x reading interrupt pipe _interruptReadPending %d - calling DecrementOutstandingIO", arg2, arg3 );
			break;

		case USB_HUB_TRACE( kTPHubDoDeviceRequest ):
			log(info, "Hub", "Do Device Request", parg1, "returning status 0x%x", arg2 );
			break;

		case USB_HUB_TRACE( kTPHubWaitForPortResumes ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Wait For Port resumes", parg1, "error 0x%x from GetPortStatus for port (%d) - setting port to active", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Wait For Port resumes", parg1, "error 0x%x from GetPortStatus for port (%d) - setting port to active", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Hub", "Wait For Port resumes", parg1, "error 0x%x from SuspendPort for port (%d)", arg2, arg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Hub", "Wait For Port resumes", parg1, "error 0x%x not sure what is going on with port number %d in OUTER loop - terminating", arg2, arg3 );
			}
			break;

		case USB_HUB_TRACE( kTPHubResetMyPort ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Reset Port", parg1, "interruptPipe->Abort returned error 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Reset Port", parg1, "port %d had the dev zero lock _hubDescriptor.numPorts %d ", arg2, arg3 );
			}
			break;
			
		case USB_HUB_TRACE( kTPHubDecrementOutstandingIO ):
			log(info, "Hub", "DecrementOutstandingIO", parg1, "_checkForActivePortsThread already queued");
			break;

		case USB_HUB_TRACE( kTPHubChangeOutstandingIO ):
			if ( arg4 == kIOReturnBadArgument )
			{
				log(info, "Hub", "ChangeOutstanding IO", parg1, "invalid target direction %d retCount %d status 0x%x", arg2, arg3, arg4 );
			}
			else
				log(info, "Hub", "ChangeOutstanding IO", parg1, "invalid direction %d error 0x%x", arg2, arg3 );
			break;

		case USB_HUB_TRACE( kTPHubChangeRaisedPowerState ):
			if ( arg4 == kIOReturnBadArgument )
			{
				log(info, "Hub", "ChangeRaisedPowerState", parg1, "invalid target direction %d retCount %d status 0x%x", arg2, arg3, arg4 );
			}
			else
				log(info, "Hub", "ChangeRaisedPowerState", parg1, "invalid direction %d error 0x%x", arg2, arg3 );
			break;

		case USB_HUB_TRACE( kTPHubChangeOutstandingResumes ):
			if ( arg4 == kIOReturnBadArgument )
			{
				log(info, "Hub", "ChangeOutstanding Resumes", parg1, "invalid target direction %d retCount %d status 0x%x", arg2, arg3, arg4 );
			}
			else
				log(info, "Hub", "ChangeOutstanding Resumes", parg1, "invalid direction %d error 0x%x", arg2, arg3 );
			break;

		case USB_HUB_TRACE( kTPHubEnterTestMode ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Enter TestMode", parg1, "root hub");
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Enter TestMode", parg1, "external hub - suspending ports _hubDescriptor.numPorts %d", arg2 );
			}
			break;

		case USB_HUB_TRACE( kTPHubLeaveTestMode ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "Leave TestMode", parg1, "root hub");
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "Leave TestMode", parg1, "external hub");
			}
			break;

		case USB_HUB_TRACE( kTPHubPutPortIntoTestMode ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "PutPortIntoTestMode", parg1, "putting root hub port %d into mode %x", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "PutPortIntoTestMode", parg1, "putting external hub port %d into mode %x", arg2, arg3 );
			}
			break;

		case USB_HUB_TRACE( kTPHubGetPortIndicatorControl ):
			log(info, "Hub", "PutPortIntoTestMode", parg1, "GetPortStatus to location %d port %d got error (0x%x) from DoDeviceRequest", arg2, arg3, arg4 );
			break;
		
		case USB_HUB_TRACE( kTPHubSetIndicatorsToAutomatic ):
			log(info, "Hub", "Set Indicators Auto", parg1, "locationID %d port %d got error 0x%x", arg2, arg3, arg4 );
			break;

		case USB_HUB_TRACE( kTPHubGetPortPower ):
			log(info, "Hub", "Get Power", parg1, "locationID %d port %d got error 0x%x", arg2, arg3, arg4 );
			break;
		
		case USB_HUB_TRACE( kTPHubEnsureUsability ):
			log(info, "Hub", "Ensure Usability", parg1, "_checkPortsThreadActive after delay!");
			break;
			
		case USB_HUB_TRACE( kTPHubWaitForPowerOn ):
			if ( arg4 == 1 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "nil workloop or nil gate");
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "called on workloop thread (this should be OK)" );
			}
			else if ( arg4 == 3 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "called inGate, but we are already waiting, returning kIOReturnInternalErr" );
			}
			else if ( arg4 == 4 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "commandSleep timed out (THREAD_TIMED_OUT) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "commandSleep interrupted (THREAD_INTERRUPTED) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "commandSleep restarted (THREAD_RESTART) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "commandSleep woke up (kIOReturnNotPermitted) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 8 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "commandSleep woke up with status (0x%x) _myPowerState(%d)", arg2, arg3 );
			}
			else if ( arg4 == 9 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "not in gate, sleeping 10 ms _myPowerState[%d] (retries = %d)", arg2, arg3);
			}
			else if ( arg4 == 10 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "successful, _myPowerState[%d], returning kIOReturnSuccess", arg2 );
			}
			else if ( arg4 == 11 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "was not successful, _myPowerState[%d], _powerStateChangingTo[%d] - returning kIOReturnInternalErr", arg2, arg3 );
			}
			else if ( arg4 == 12 )
			{
				log(info, "Hub", "WaitForPowerOn", parg1, "commandSleep woke up normally (THREAD_AWAKENED) _myPowerState(%d)", arg2);
			}
			break;
			
		case USB_HUB_TRACE( kTPHubDoPortActionLock ):
			if (arg4 == 0)
			{
				log(info, "Hub", "DoPortActionLock", parg1, "_doPortActionLock available without commandSleep");
			}
			else if ( arg4 == 1 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "_doPortActionLock held by someone else - calling commandSleep to wait for lock");
			}
			else if ( arg4 == 2 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "commandSleep woke up normally (THREAD_AWAKENED) _doPortActionLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 3 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "commandSleep interrupted (THREAD_INTERRUPTED) _doPortActionLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 4 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "commandSleep restarted (THREAD_RESTART) _doPortActionLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 5 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "woke up with status (kIOReturnNotPermitted) - we do not hold the WL!");
			}
			else if ( arg4 == 6 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "woke up with unknown status 0x%x", arg2);
			}
			else if ( arg4 == 7 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "commandSleep timeout out (THREAD_TIMED_OUT) _doPortActionLock(%s)", arg2 ? "true" : "false");
			}
			else if ( arg4 == 8 )
			{
				log(info, "Hub", "DoPortActionLock", parg1, "setting _doPortActionLock to false and calling commandWakeup");
			}
			break;
			
		case USB_HUB_TRACE( kTPHubCheckForDeadDevice ):
			if ( arg4 == 0 )
			{
				log(info, "Hub", "CheckForDeadHub", parg1, "we are inActive(), so ignoring");
			}
			else if ( arg4 == 1)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "hubIsDead is true, returning");
			}
			else if ( arg4 == 2)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "_retryCount: %d, _deviceHasBeenDisconnected = %d", arg2, arg3);
			}
			else if ( arg4 == 3)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "GetDeviceInformation returned error 0x%x, _retryCount: %d", arg2, arg3);
			}
			else if ( arg4 == 4)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "GetDeviceInformation returned error and our retryCount is 0, so assume device has been unplugged");
			}
			else if ( arg4 == 5)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "GetDeviceInformation returned error and our retryCount is not 0");
			}
			else if ( arg4 == 6)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "GetDeviceInformation returned info: 0x%x, retryCount: %d", arg2, arg3);
			}
			else if ( arg4 == 7)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "Still connected but retry count (%d) not reached, clearing stall and retrying", arg2);
			}
			else if ( arg4 == 8)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "device(%p) has been unplugged", (void *) arg2);
			}
			else if ( arg4 == 9)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "no hubInterface (%p), device(%p), or pipe", (void*)arg2, (void*)arg3);
			}
			else if ( arg4 == 10)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "already active, returning");
			}
			else if ( arg4 == 11)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "_hubDeadCheckLock was not set.  Unexpected");
			}
			else if ( arg4 == 12)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "Still connected and retry count reached, calling ResetMyPort()");
			}
			else if ( arg4 == 13)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "device is still connected (and enabled) _retryCount: %d", arg2);
			}
			else if ( arg4 == 14)
			{			   
				log(info, "Hub", "CheckForDeadHub", parg1, "device is still connected (but NOT enabled) calling ResetMyPort()");
			}
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceHubPort ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_HUB_PORT_TRACE( kTPHubPortStop ):
			if ( arg4 == 0 )
			{
				log(info, "HubPort", "Stop", parg1, "Error 0x%x from ClearPortFeature(0x%x)", arg2, arg3 );
			}
			else if ( arg4 == 1 )
			{
				log(info, "HubPort", "Stop", parg1, "Port %d of Hub at 0x%x ", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "HubPort", "Stop", parg1, "Port %d of Hub at 0x%x  no workloop", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "HubPort", "Stop", parg1, "Port %d of Hub at 0x%x  got the WL but there is no gate", arg2, arg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "HubPort", "Stop", parg1, "Port %d of Hub at 0x%x  on the main thread. DANGER AHEAD", arg2, arg3 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "HubPort", "Stop", 0, "Port %d of Hub at 0x%x  sleeping for 100ms while retries (%d)", arg1, arg2, arg3 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "HubPort", "Stop", 0, "Port %d of Hub at 0x%x  commandSleep(&_statusChangedThreadActive) returned 0x%x", arg2, arg3, arg1 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "HubPort", "Stop", 0, "Port %d of Hub at 0x%x  commandSleep(&_initThreadActive) returned 0x%x", arg2, arg3, arg1 );
			}
			else if ( arg4 == 8 )
			{
				log(info, "HubPort", "Stop", 0, "Port %d of Hub at 0x%x  commandSleep(&_addDeviceThreadActive) returned 0x%x", arg2, arg3, arg1 );
			}
			else if ( arg4 == 9 )
			{
				log(info, "HubPort", "Stop", 0, "Port %d of Hub at 0x%x  commandSleep(&_enablePowerAfterOvercurrentThreadActive) returned 0x%x", arg2, arg3, arg1 );
			}
			else if ( arg4 == 10 )
			{
				log(info, "HubPort", "Stop", 0, "Port %d of Hub at 0x%x  woke up from commandSleep (%d/%d/%d/%d)", arg1, arg2, arg3>>3, arg3>>2, arg3>>1, arg3&1 );
			}
			else if ( arg4 == 11 )
			{
				log(info, "HubPort", "Stop", parg1, "Port %d of Hub at 0x%x  not quiesced.  Just returning!", arg2, arg3 );
			}
			else if ( arg4 == 12 )
			{
				log(info, "HubPort", "Stop", parg1, "Port %d of Hub at 0x%x  had devZero, releasing", arg2, arg3 );
			}
			else if ( arg4 == 13 )
			{
				log(info, "HubPort", "Stop", 0, "Port %d of Hub at 0x%x  trying comandSleep (%d/%d/%d/%d)", arg1, arg2, arg3>>3, arg3>>2, arg3>>1, arg3&1 );
			}
			break;
		
		case USB_HUB_PORT_TRACE( kTPHubPortAddDevice ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "AddDevice", 0, "port %d on hub 0x%x - unable (error = 0x%x) to reset port (set feature (resetting port)", arg1, arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "HubPort", "AddDevice", 0, "Port %d of Hub at 0x%x locationID 0x%x ", arg1, arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "HubPort", "AddDevice", parg1, "port %d not in reset after 5 retries ", arg2 );
			}
			break;

		case USB_HUB_PORT_TRACE( kTPHubPortSuspendPort ):
			log(info, "HubPort", "SuspendPort", parg1, "RESUME - _lowerPowerStateOnResume already set (hub 0x%x port %d)- UNEXPECTED!", arg2, arg3 );
			break;


		case USB_HUB_PORT_TRACE( kTPHubPortFatalError ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "FatalError", 0, "error 0x%x Port %d of Hub at 0x%x", arg1, arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "HubPort", "FatalError", 0, "device 0x%x Port %d of Hub at 0x%x", arg1, arg2, arg3 );
			}
			break;

		case USB_HUB_PORT_TRACE( kTPHubPortAddDeviceResetChangeHandler ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "Dev Reset ChangeHlr", parg1, "Port %d of Hub at 0x%x,  we have a hub, but this would be the 6th hub in the bus, which is illegal.  Erroring out", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "HubPort", "Dev Reset ChangeHlr", parg1, "we could not get the kIOUSBPlane 0x%x !!  Problems ahead", arg2 );
			}
			else
				log(info, "HubPort", "Dev Reset ChangeHlr", 0, "Port %d of Hub at 0x%x, unable to set device 0x%x to address %d - disabling port", arg1, arg2, arg3, arg4 );
			break;

		case USB_HUB_PORT_TRACE( kTPHubPortHandleResetPortHandler ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "HandleResetPortHandler", parg1, "port %d, unable (error = 0x%x) to disable port", arg2 , arg3);
			}
			else
				log(info, "HubPort", "HandleResetPortHandler", parg1, "Port %d of Hub at 0x%x, unable to set address %d - disabling port", arg2, arg3, arg4 );
			break;
			
		case USB_HUB_PORT_TRACE( kTPHubPortDefaultOverCrntChangeHandler ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "DefaultOverCrntChangeHandler", parg1, "OverCurrent condition in Port %d of hub @ 0x%x", arg2, arg3 );
			}
			if ( arg4 == 2 )
			{
				log(info, "HubPort", "DefaultOverCrntChangeHandler", parg1, "No OverCurrent condition. Ignoring. Port %d of hub @ 0x%x", arg2, arg3 );
			}
			if ( arg4 == 3 )
			{
				log(info, "HubPort", "DefaultOverCrntChangeHandler", parg1, "cannot call out to EnablePowerAfterOvercurrentThread for Port %d of hub @ 0x%x", arg2, arg3 );
			}
			break;

		case USB_HUB_PORT_TRACE( kTPHubPortDefaultConnectionChangeHandler ):
			log(info, "HubPort", "ConnectionChangeHandler", parg1, "IOUSBFamily:  Ignoring a false disconnect after wake for the device port 0x%x locationID 0x%x", arg2, arg3 );
			break;

		case USB_HUB_PORT_TRACE( kTPHubPortReleaseDevZeroLock ):
			log(info, "HubPort", "Rel Dev Zero Lock", parg1, "ReleaseDevZeroLock() port %d of hub @ 0x%x, devzero %d", arg2, arg4, arg3 );
			break;

		case USB_HUB_PORT_TRACE( kTPHubPortDetachDevice ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "DetachDevice", parg1, "Port %d of Hub at 0x%x being detached", arg2, arg3 );
			}
			else if ( arg4 == kIOReturnNoDevice )
			{
				log(info, "HubPort", "DetachDevice", parg1, "Port %d of Hub at 0x%x - device has gone away status 0x%x", arg2, arg3, arg4 );
			}
			else
				log(info, "HubPort", "DetachDevice", parg1, "Port %d of Hub at 0x%x), attachRetry limit reached. delaying for %d milliseconds", arg2, arg3, arg4 );
			break;

		case USB_HUB_PORT_TRACE( kTPHubPortGetDevZeroDescriptorWithRetries ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "Get Dev Zero Desc Retry", parg1, "port %d of hub @ 0x%x - GetDeviceZeroDescriptor returned kIOReturnOverrun.  Checking for valid descriptor", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "HubPort", "Get Dev Zero Desc Retry", parg1, "port %d of hub @ 0x%x - GetDeviceZeroDescriptor.  Descriptor looks valid", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "HubPort", "Get Dev Zero With Retry", parg1, "port %d - GetDeviceZeroDescriptor returned status 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "HubPort", "Get Dev Zero With Retry", parg1, "port %d - GetPortStatus returned 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "HubPort", "Get Dev Zero With Retry", parg1, "port %d of hub @ 0x%x, - port is suspended", arg2, arg3 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "HubPort", "Get Dev Zero With Retry", parg1, "port %d of hub @ 0x%x, - bad USB descriptor", arg2, arg3 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "HubPort", "Get Dev Zero With Retry", parg1, "port %d aborting due to power change to %d ", arg2, arg3 );
			}
			else
				log(info, "HubPort", "Get Dev Zero With Retry", parg1, "port %d - device has gone away state %d status 0x%x", arg2, arg3, arg4 );
			break;
		
		case USB_HUB_PORT_TRACE( kTPHubPortDisplayOverCurrentNotice ):
			if (arg4 == 1)
			{
				log(info, "HubPort", "Disp OverCurrent", parg1, "port %d - individual: %d", arg2, arg3 );
			}
			else if (arg4 == 2)
				log(info, "HubPort", "Disp OverCurrent", parg1, "_hub 0x%x or _hub->_device is NULL", arg2 );
			break;
			
		case USB_HUB_PORT_TRACE( kTPHubPortWaitForSuspendCommand ):
			if ( arg4 == 1 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "nil workloop or nil gate");
			}
			else if ( arg4 == 2 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "called on workloop thread (this should be OK)" );
			}
			else if ( arg4 == 3 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "commandSleep woke up normally (THREAD_AWAKENED) _myPowerState(%d)", arg2);
			}
			else if ( arg4 == 4 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "commandSleep timed out (THREAD_TIMED_OUT) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "commandSleep interrupted (THREAD_INTERRUPTED) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "commandSleep restarted (THREAD_RESTART) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "commandSleep woke up (kIOReturnNotPermitted) _myPowerState(%d)", arg2 );
			}
			else if ( arg4 == 8 )
			{
				log(info, "HubPort", "WaitForSuspendCommand", parg1, "commandSleep woke up with status (0x%x) _myPowerState(%d)", arg2, arg3 );
			}

			break;
			
		case USB_HUB_PORT_TRACE( kTPHubPortWakeSuspendCommand ):
			if (arg4 == 1)
			{
				log(info, "HubPort", "WakeSuspendCommand", parg1, "nil workloop or nil gate");
			}
			else if (arg4 == 2)
				log(info, "HubPort", "WakeSuspendCommand", parg1, "cannot call commandGate->wakeup because there is no gate");
			break;
			
		case USB_HUB_PORT_TRACE( kTPHubPortEnablePowerAfterOvercurrent ):
			if (arg4 == 0)
			{
				log(info, "HubPort", "EnablePowerAfterOvercurrent", 0, "Port %d of Hub @ 0x%x, status: 0x%x, change: 0x%x", arg1, arg2, arg3>>16, arg3 & 0xFFFF);
			}
			else if (arg4 == 1)
			{
				log(info, "HubPort", "EnablePowerAfterOvercurrent", 0, "Enabling Power for Port %d of Hub @ 0x%x, _portPMState = %d", arg2, arg1, arg3);
			}
			else if (arg4 == 2)
			{
				log(info, "HubPort", "EnablePowerAfterOvercurrent", 0, "Error: 0x%x Port %d of Hub @ 0x%x", arg3, arg2, arg1);
			}
			else if (arg4 == 3)
			{
				log(info, "HubPort", "EnablePowerAfterOvercurrent", arg1, "Port %d of Hub @ 0x%x", arg2, arg3);
			}
			else if (arg4 == 4)
			{
				log(info, "HubPort", "EnablePowerAfterOvercurrent", arg1, "Port %d of Hub @ 0x%x, calling commandWake()", arg2, arg3);
			}
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceHSHubUserClient ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_HUB_UC_TRACE( kTPHSHubUCInitWithTask ):
			log(info, "HSHubUC", "initWithTask", parg1, "owningTask 0x%x security_id 0x%x initWithTask(type %d)", arg2, arg3, arg4 );
			break;
		
		case USB_HUB_UC_TRACE( kTPHSHubUCStart ):
			log(info, "HSHubUC", "Start", parg1, "start fOwner 0x%x", arg2 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCClose ):
			log(info, "HSHubUC", "Close", parg1, "close fOwner 0x%x", arg2 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCGetNumberOfPorts ):
			log(info, "HSHubUC", "GetNumberOfPorts", parg1, "returning fOwner 0x%x ports %d", arg2, arg3 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCGetLocationID ):
			log(info, "HSHubUC", "GetLocationID", parg1, "returning fOwner 0x%x locationID %d", arg2, arg3 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCPutPortIntoTestMode ):
			if ( arg3 == kIOReturnBadArgument )
			{
				log(info, "HSHubUC", "PutPortIntoTestMode", parg1, "fOwner 0x%x is not open", arg2 );
			}
			else
				log(info, "HSHubUC", "PutPortIntoTestMode", parg1, "returning fOwner 0x%x port %d mode %d", arg2, arg3, arg4 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCSupportsIndicators ):
			log(info, "HSHubUC", "SupportsIndicators", parg1, "returning fOwner %d indicatorSupport %d", arg2, arg3 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCSetIndicatorForPort ):
			if ( arg3 == kIOReturnBadArgument )
			{
				log(info, "HSHubUC", "SetIndicatorForPort", parg1, "fOwner 0x%x is not open returning status 0x%x ", arg2, arg3 );
			}
			else
				log(info, "HSHubUC", "SetIndicatorForPort", parg1, "fOwner 0x%x port %d selector %d",  arg2, arg3, arg4 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCGetPortIndicatorControl ):
			if ( arg3 == kIOReturnBadArgument )
			{
				log(info, "HSHubUC", "SetIndicatorForPort", parg1, "fOwner 0x%x is not open returning status 0x%x", arg2, arg3 );
			}
			else
				log(info, "HSHubUC", "SetIndicatorForPort", 0, "fOwner 0x%x port %d defaultColors %d status 0x%x",  arg1, arg2, arg3, arg4 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCSetIndicatorsToAutomatic ):
			if ( arg3 == kIOReturnBadArgument )
			{
				log(info, "HSHubUC", "SetIndicators Auto", parg1, "fOwner 0x%x is not open returning status 0x%x", arg2, arg3 );
			}else
				log(info, "HSHubUC", "SetIndicators Auto", parg1, "fOwner 0x%x ", arg2 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCGetPowerSwitchingMode ):
			log(info, "HSHubUC", "GetPowerSwitchingMode", parg1, "fOwner 0x%x returning %d", arg2, arg3 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCGetPortPower ):
			if ( arg3 == kIOReturnBadArgument )
			{
				log(info, "HSHubUC", "Get Port Power", parg1, "fOwner 0x%x is not open returning status 0x%x", arg2, arg3 );
			}
			else
				log(info, "HSHubUC", "Get Port Power", 0, "fOwner 0x%x port %d on %d status 0x%x ", arg1, arg2, arg3, arg4 );
			break;

		case USB_HUB_UC_TRACE( kTPHSHubUCSetPortPower ):
			if ( arg3 == kIOReturnBadArgument )
			{
				log(info, "HSHubUC", "Set Port Power", parg1, "fOwner 0x%x is not open returning status 0x%x", arg2, arg3 );
			}
			else
				log(info, "HSHubUC", "Set Port Power", 0, "fOwner 0x%x port %d on %d ", arg1, arg2, arg3 );
			break;
		
		case USB_HUB_UC_TRACE( kTPHSHubUCDisablePwrMgmt ):
			if ( arg2 == kIOReturnBadArgument )
			{
				log(info, "HSHubUC", "DisablePowerMgmt", parg1, "fOwner 0x%x is not open returning status 0x%x", arg2, arg3 );
			}
			else
				log(info, "HSHubUC", "DisablePowerMgmt", 0, "fOwner 0x%x disable %d ", arg1, arg2 );
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceHID	( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_HID_TRACE( kTPHIDStart ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "HID", "start", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "HID", "end", parg1, NULL );
			}
			else
			{
				if ( arg4 == 1 )
				{
					log(info, "HID", "Start", parg1, "super::start returned false!");
				}
				else if ( arg4 == 2 )
				{					
					log(info, "HID", "Start", parg1, "could not get a command gate");
				}
				else if ( arg4 == 3 )
				{					
					log(info, "HID", "Start", parg1, "unable to find my workloop");
				}
				else if ( arg4 == 4 )
				{					
					log(info, "HID", "Start", parg1, "unable to add gate to work loop");
				}
				else if ( arg4 == 5 )
				{					
					log(info, "HID", "Start", parg1, "unable to get interrupt pipe");
				}
				else if ( arg4 == 6 )
				{					
					log(info, "HID", "Start", parg1, "unable to get create buffer");
				}
				else if ( arg4 == 7 )
				{					
					log(info, "HID", "Start", parg1, "could not allocate all thread functions");
				}
				else if ( arg4 == 8 )
				{					
					log(info, "HID", "Start", parg1, "error in StartFinalProcessing");
				}
				else if ( arg4 == 9 )
				{					
					log(info, "HID", "Start", parg1, "location @ 0x%x aborting startup", arg2 );
				}
				else if ( arg4 == 10 )
				{					
					log(info, "HID", "Start", parg1, "error 0x%x from InitializeUSBHIDPowerManagement", arg2 );
				}
				else
					log(info, "HID", "Start", 0, "USB HID Interface #%d of device @ %d (0x%x)",  arg1, arg2, arg3 ); 
			}
			break;

		case USB_HID_TRACE( kTPHIDpowerStateWillChangeTo ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "HID", "PowerStateWillChangeTo Start", parg1, "capabilities %d stateNumber %d whatDevice 0x%x ", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "HID", "PowerStateWillChangeTo End", parg1, "stateNumber %d PMState 0x%x ", arg2, arg3 );
			}
			else
			{
	        	log(info, "HID", "PowerStateWillChangeTo", parg1, "while inactive - ignoring PMState 0x%x ", arg2 );
			}
			break;

		case USB_HID_TRACE( kTPHIDsetPowerState ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "HID", "SetPower State Start", parg1, "powerStateOrdinal %d whatDevice 0x%x ", arg2, arg3 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "HID", "SetPower State End", parg1, NULL );
			}
			else
			{
				if ( arg4 == 1 )
				{
					log(info, "HID", "SetPower State", parg1, "whatDevice != this");
				}
				else if ( arg4 == 2 )
				{
					log(info, "HID", "SetPower State", parg1, "bad ordinal %d ", arg2 );
				}
				else if ( arg4 == 3 )
				{					
					log(info, "HID", "SetPower State", parg1, "unknown ordinal %d ", arg2 );
				}
			}
			break;
		

		case USB_HID_TRACE( kTPHIDpowerStateDidChangeTo ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "HID", "powerStateDidChangeTo start", parg1, "capabilities %d stateNumber %d whatDevice 0x%x ", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "HID", "powerStateDidChangeTo end", parg1, NULL );
			}
			else
			{
	        	log(info, "HID", "powerStateDidChangeTo", parg1, "error 0x%x returned from RearmInterruptRead", arg2 );
			}
			break;

		case USB_HID_TRACE( kTPHIDpowerChangeDone ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "HID", "powerChangeDone start", parg1, "fromstate %d ", arg2 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "HID", "powerChangeDone end", parg1, NULL );
			}
			else
			{
	        	log(info, "HID", "powerChangeDone", parg1, "from state %d - ignoring", arg2 );
			}
			break;
		

		case USB_HID_TRACE( kTPHIDhandleStart ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "HID", "handlestart start", parg1, "provider 0x%x ", arg2 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "HID", "handlestart end", parg1, "interface %d device %d ", arg2, arg3 );
			}
			else
			{
				if ( arg4 == 1 )
				{
				   	log(info, "HID", "handlestart", parg1, "unable to open provider. returning false");
				}
				else if ( arg4 == 2 )
				{				   	
					log(info, "HID", "handlestart", parg1, "Cannot get our provider's USB device.  This is bad");
				}
				else if ( arg4 == 3 )
				{
				   	log(info, "HID", "handlestart", parg1, "Our provider is not an IOUSBInterface!!");
				}
			}
			break;

		case USB_HID_TRACE( kTPHIDClearFeatureEndpointHalt ):
	   		log(info, "HID", "Clear Feature EP Halt", parg1, "no interrupt pipe - bailing");
			break;
		
		case USB_HID_TRACE( kTPHIDHandleReport ):
	   		log(info, "HID", "HandleReport", parg1, "handleReportWithTime() returned 0x%x", arg2 );
			break;

		case USB_HID_TRACE( kTPHIDSuspendPort ):
			if ( arg4 == 1 )
			{			   	
				log(info, "HID", "SuspendPort", parg1, "could not create _SUSPENDPORT_TIMER error 0x%x", arg2 );
			}
			else if ( arg4 == 2 )
			{			   	
				log(info, "HID", "SuspendPort", parg1, "addEventSource returned status 0x%x", arg2  );
			}
			else if ( arg4 == 3 )
			{			   	
				log(info, "HID", "SuspendPort", parg1, "Our provider is not an IOUSBInterface!! error 0x%x", arg2 );
			}
			break;

		case USB_HID_TRACE( kTPHIDClaimPendingRead ):
			if ( arg4 == 1 )
			{			   
				log(info, "HID", "SuspendPort", parg1, "me -> NULL invalid target", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{			   	
				log(info, "HID", "SuspendPort", parg1, "NULL retVal!!", arg2, arg3  );
			}
			break;
			

		case USB_HID_TRACE( kTPHIDChangeOutstandingIO ):
			if ( arg4 == 1 )
			{
				log(info, "HID", "ChangeOutstandingIO", parg1, "invalid target status 0x%x direction %d", arg2, arg3 );
			}
			else
			   	log(info, "HID", "ChangeOutstandingIO", parg1, "invalid direction outstandingIO %d status 0x%x direction %d", arg2, arg3, arg4  );
			break;

		case USB_HID_TRACE( kTPHIDAbortAndSuspend ):
			log(info, "HID", "Abort&Suspend", parg1, "resuming the device returned 0x%x", arg2 );
			break;

		case USB_HID_TRACE( kTPHIDRearmInterruptRead ):
			if ( arg4 == 0 )
			{
				log(info, "HID", "Interrupt Read", parg1, "error 0x%x, bufferSizeRemaining: %d", arg2, arg3);
			}
			if ( arg4 == 1 )
			{			   	
				log(info, "HID", "RearmInterruptRead", parg1, "no _buffer or _interruptPipe");
			}
			else if ( arg4 == 2 )
			{
				log(info, "HID", "RearmInterruptRead", parg1, "no action method");
			}
			else if ( arg4 == 3 )
			{
			   	log(info, "HID", "RearmInterruptRead", parg1, "unable to check for pending");
			}
			else if ( arg4 == 4 )
			{			   	
				log(info, "HID", "RearmInterruptRead", parg1, "immediate error 0x%x queueing read, clearing stall and trying again(%d)", arg2, arg3 );
			}
			else if ( arg4 == 5 )
			{			   	
				log(info, "HID", "RearmInterruptRead", parg1, "returning error 0x%x", arg2 );
			}
			else if ( arg4 == 6 )
			{			   
				log(info, "HID", "RearmInterruptRead", parg1, "isInactive" );
			}
			else if ( arg4 == 7 )
			{			   
				log(info, "HID", "RearmInterruptRead", parg1, "error (kIOReturnNotResponding) while _POWERSTATECHANGING(%s) or (_MYPOWERSTATE < kUSBHIDPowerStateOn)(%d) - no posting read", arg2 ? "true" : "false", arg3 );
			}
			else if ( arg4 == 8 )
			{			   
				log(info, "HID", "RearmInterruptRead", parg1, "error (kIOReturnNotResponding) while _POWERSTATECHANGING(%s) or (_MYPOWERSTATE < kUSBHIDPowerStateOn)(%d) - no posting read", arg2 ? "true" : "false", arg3 );
			}
			break;

		case USB_HID_TRACE( kTPHIDInterruptReadError ):
			if (arg4 == kIOReturnAborted)
			{
				log(info, "HID", "Interrupt Read Error", parg1, "Abort received. Expected(%s) Queuing Another(%s)", arg2 ? "true" : "false", arg3 ? "true" : "false");
			}
			break;
			
		case USB_HID_TRACE( kTPHIDInterruptRead ):
			if ( arg4 == 0 )
			{
				log(info, "HID", "InterruptRead", parg1, "bytes read: %d (of %d)", arg2, arg3);
			}
			else if ( arg4 == 1)
			{			   
				log(info, "HID", "InterruptRead", parg1, "error: 0x%x, _deviceHasBeenDisconnected = %d", arg2, arg3);
			}
			else if ( arg4 == 3)
			{			   
				log(info, "HID", "InterruptRead", parg1, "_HANDLE_REPORT_THREAD was already queued");
			}
			else if ( arg4 == 4)
			{			   
				log(info, "HID", "InterruptRead", parg1, "no data in buffer, just re-queueing");
			}
			else if ( arg4 == 5)
			{			   
				log(info, "HID", "InterruptRead", parg1, "Not calling handleReport thread because we already have a report queued");
			}
			else if ( arg4 == 6)
			{			   
				log(info, "HID", "InterruptRead", parg1, "kIOReturnNotResponding or kIOUSBHighSpeedSplitError error but port is suspended");
			}
			else if ( arg4 == 7)
			{			   
				log(info, "HID", "InterruptRead", parg1, "Checking to see if HID device is still connected");
			}
			else if ( arg4 == 8)
			{			   
				log(info, "HID", "InterruptRead", parg1, "_deviceDeadCheckThread was already queued");
			}
			else if ( arg4 == 9)
			{			   
				log(info, "HID", "InterruptRead", parg1, "error kIOReturnAborted. Try again");
			}
			else if ( arg4 == 10)
			{			   
				log(info, "HID", "InterruptRead", parg1, "error kIOReturnAborted. Expected.  Not rearming interrupt");
			}
			else if ( arg4 == 11)
			{			   
				log(info, "HID", "InterruptRead", parg1, "other error, clearing up stalls and retrying");
			}
			else if ( arg4 == 12)
			{			   
				log(info, "HID", "InterruptRead", parg1, "Unknown error (0x%x) reading interrupt pipe", arg2);
			}
			break;
			
		case USB_HID_TRACE( kTPHIDInitializeUSBHIDPowerManagement ):
			log(info, "HID", "InitializeUSBHIDPowerManagement", parg1, "error 0x%x from registerPowerDriver", arg2 );
			break;
			
		case USB_HID_TRACE( kTPHIDCheckForDeadDevice ):
			if ( arg4 == 1 )
			{
				log(info, "HID", "CheckForDeadDevice", parg1, "_retryCount: %d, _deviceHasBeenDisconnected = %d, already active, returning", arg2, arg3);
			}
			else if ( arg4 == 2)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "_retryCount: %d, _deviceHasBeenDisconnected = %d", arg2, arg3);
			}
			else if ( arg4 == 3)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "GetDeviceInformation returned error 0x%x, _retryCount: %d", arg2, arg3);
			}
			else if ( arg4 == 4)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "GetDeviceInformation returned error and our retryCount is 0, so assume device has been unplugged");
			}
			else if ( arg4 == 5)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "GetDeviceInformation returned info: 0x%x, retryCount: %d", arg2, arg3);
			}
			else if ( arg4 == 6)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "device is still connected (and enabled) _retryCount: %d", arg2);
			}
			else if ( arg4 == 7)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "about to call ResetDevice()");
			}
			else if ( arg4 == 8)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "device %p has been unplugged", (void *)parg2);
			}
			else if ( arg4 == 9)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "_device or _interface were NULL");
			}
			else if ( arg4 == 10)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "our _DEAD_DEVICE_CHECK_LOCK was not set.  Unexpected");
			}
			else if ( arg4 == 11)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "Still connected but retry count (%d) not reached", arg2);
			}
			else if ( arg4 == 12)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "we are inActive(), so ignoring");
			}
			else if ( arg4 == 13)
			{			   
				log(info, "HID", "CheckForDeadDevice", parg1, "GetDeviceInformation returned an error, but our retryCount is not 0 (%d)", arg2);
			}
			break;
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTracePipe ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
		
	switch ( type )
	{
		case USB_PIPE_TRACE( kTPPipeInitToEndpoint ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "InitToEndpoint start", parg1, "ed 0x%x speed %d address 0x%x ", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "InitToEndpoint end", parg1, NULL );
			}
			else
			{
	        	log(info, "Pipe", "InitToEndpoint", parg1, "_controller 0x%x _endpoint.transferType %d _endpoint.maxPacketSize %d", arg2, arg3, arg4 );
			}
			break;

		case USB_PIPE_TRACE( kTPIsocPipeRead ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "Isoc PipeRead start", parg1, "buffer 0x%x frameStart %d numFrames %d ", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "Isoc PipeRead end", parg1, "error 0x%x ", arg2 );
			}
			else
			{
	        	log(info, "Pipe", "Isoc PipeRead", parg1, "completion has NULL action - returning 0x%x ", arg2 );
			}
			break;
		
		case USB_PIPE_TRACE( kTPIsocPipeWrite ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "Isoc PipeWrite start", parg1, "buffer 0x%x frameStart %d numFrames %d ", arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "Isoc PipeWrite end", parg1, "error 0x%x ", arg2 );
			}
			else
			{
	        	log(info, "Pipe", "Isoc PipeWrite", parg1, "completion has NULL action - returning 0x%x ", arg2 );
			}
			break;

		case USB_PIPE_TRACE( kTPPipeControlRequestMemDesc ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "Control Req MemDesc start", 0, "bmRequestType  0x%x bRequest 0x%x wValue 0x%x wIndex %d ", arg1, arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "Control Req MemDesc end", parg1, "error 0x%x ", arg2 );
			}
			else
			{
				if ( arg4 == 1 )
				{		        	
					log(info, "Pipe", "Control Req MemDesc", parg1, "completion has NULL action - returning 0x%x ", arg2 );
				}
				else if ( arg4 == 2 )
				{		        	
					log(info, "Pipe", "Control Req MemDesc", parg1, "completion has NULL action - returning 0x%x ", arg2 );
				}
			}
			break;
		
		case USB_PIPE_TRACE( kTPPipeControlRequest ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "Control Req start", 0, "bmRequestType  0x%x bRequest 0x%x wValue 0x%x wIndex %d ", arg1, arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "Control Req end", parg1, "error 0x%x ", arg2 );
			}
			break;
		
		case USB_PIPE_TRACE( kTPBulkPipeRead ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "Read start", 0, "USB Address: %d, EP: %d %s transaction, reqCount %d ", arg1, arg2, DecodeUSBTransferType(arg3), arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "Read end", parg1, "error 0x%x ", arg2 );
			}
			else
			{
	        	log(info, "Pipe", "Read", parg1, "completion has NULL action - returning status 0x%x ", arg2 );
			}
			break;

		case USB_PIPE_TRACE( kTPBulkPipeWrite ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "Write start", 0, "USB Address: %d, EP: %d %s transaction, reqCount %d ", arg1, arg2, DecodeUSBTransferType(arg3), arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "Write end", parg1, "error 0x%x ", arg2 );
			}
			else
			{
	        	log(info, "Pipe", "Write", parg1, "completion has NULL action - returning status 0x%x ", arg2 );
			}
			break;

		case USB_PIPE_TRACE( kTPIsocPipeReadLL ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "IsocRead LL start", 0, "buffer 0x%x completion 0x%x numFrames %d updateFrequency %d ", arg1, arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "IsocRead LL end", parg1, "error 0x%x ", arg2 );
			}
			else
			{
	        	log(info, "Pipe", "IsocRead LL", parg1, "completion has NULL action - returning status 0x%x ", arg2 );
			}
			break;

		case USB_PIPE_TRACE( kTPIsocPipeWriteLL ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "IsocWrite LL start", 0, "buffer 0x%x completion 0x%x numFrames %d updateFrequency %d ", arg1, arg2, arg3, arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "IsocWrite LL end", parg1, NULL );
			}
			else
			{
	        	log(info, "Pipe", "IsocWrite LL", parg1, "completion has NULL action - returning status 0x%x ", arg2 );
			}
			break;

		case USB_PIPE_TRACE( kTPIBulkReadTS ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "Pipe", "ReadTS start", 0, "USB Address: %d, EP: %d %s transaction, reqCount %d ", arg1, arg2, DecodeUSBTransferType(arg3), arg4 );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
	        	log(info, "Pipe", "ReadTS end", parg1, NULL );
			}
			else
			{
	        	log(info, "Pipe", "ReadTS", parg1, "completionWithTimeStamp has NULL action - returning status 0x%x ", arg2 );
			}
			break;

		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceInterfaceUserClient	( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	

	switch ( type )
	{
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCReadPipe ):
			if ( arg4 == kIOReturnBadArgument ) 
			{
	        	log(info, "InterfaceUserClient", "ReadPipe", parg1, "bad arguments size %d buffer 0x%x completion 0x%x status 0x%x ", arg1, arg2, arg3, arg4 );
			} 
			else if ( arg4 == kIOReturnNoMemory ) 
			{
	        	log(info, "InterfaceUserClient", "ReadPipe", parg1, "IOMemoryDescriptor::withAddressRange returned NULL size %d memory 0x%x status 0x%x", arg2, arg3, arg4 );
			}
			break;
		
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCWritePipe ):
			if ( arg4 == kIOReturnBadArgument ) 
			{
	        	log(info, "InterfaceUserClient", "WritePipe", parg1, "bad arguments size %d buffer 0x%x completion 0x%x status 0x%x ", arg1, arg2, arg3, arg4 );
			} 
			else if ( arg4 == kIOReturnNoMemory ) 
			{
	        	log(info, "InterfaceUserClient", "WritePipe", parg1, "IOMemoryDescriptor::withAddressRange returned NULL size %d memory 0x%x status 0x%x", arg2, arg3, arg4 );
			}
			else
	        	log(info, "InterfaceUserClient", "WritePipe", parg1, "> (sync > 4K) mem->prepare() returned status 0x%x", arg2 );
			break;
		
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCControlRequestOut ):
			if ( arg4 == kIOReturnBadArgument ) 
			{
	        	log(info, "InterfaceUserClient", "ControlRequestOut", parg1, "completion is NULL! bmRequestType 0x%x bRequest 0x%x wValue %d status 0x%x ", arg1, arg2, arg3, arg4 );
			} 
			else if ( arg4 == kIOReturnNoMemory ) 
			{
	        	log(info, "InterfaceUserClient", "ControlRequestOut", parg1, "IOMemoryDescriptor::withAddressRange returned NULL memory 0x%x size %d status 0x%x", arg2, arg3, arg4 );
			}
			break;
		
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCControlRequestIn ):
			if ( arg4 == kIOReturnBadArgument ) 
			{
	        	log(info, "InterfaceUserClient", "ControlRequestIn", parg1, "bad arguments size %d buffer 0x%x completion 0x%x status 0x%x ", arg1, arg2, arg3, arg4 );
			} 
			else if ( arg4 == kIOReturnNoMemory ) 
			{
	        	log(info, "InterfaceUserClient", "ControlRequestIn", 0, "IOMemoryDescriptor::withAddressRange returned NULL size %d  buffer 0x%x memory 0x%x status 0x%x", arg1, arg2, arg3, arg4 );
			}
			break;
		
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCDoIsochPipeAsync ):
			if ( arg4 == 1 ) 
			{
				log(info, "InterfaceUserClient", "DoIsoc PipeAsync", parg1, "could not create countMem descriptor bufSize %d status 0x%x", arg2, arg3 );  
			}
			else if ( arg4 == 2 ) 
			{
				log(info, "InterfaceUserClient", "DoIsoc PipeAsync", parg1, "could not prepare dataMem descriptor bufSize %d status 0x%x", arg2, arg3 );  
			}
			else if ( arg4 == 3 ) 
			{
				log(info, "InterfaceUserClient", "DoIsoc PipeAsync", parg1, "could not create dataMem descriptor frameLen %d status 0x%x", arg2, arg3 );  
			}
			else if ( arg4 == 4 ) 
			{
				log(info, "InterfaceUserClient", "DoIsoc PipeAsync", parg1, "could not prepare dataMem descriptor countMem 0x%x status 0x%x", arg2, arg3 );  
			}
			break;

		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCLowLatencyPrepareBuffer ):
			if ( arg4 == 1 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not malloc buffer info sizeof(IOUSBLowLatencyUserClientBufferInfoV3) %d status 0x%x", arg2, arg3 );  
			}
			else if ( arg4 == 2 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not create a physically contiguous IOBMD size %d status 0x%x", arg2, arg3 );  
			}
			else if ( arg4 == 3 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not prepare the data buffer memory descriptor 0x%x status 0x%x", arg2, arg3 );  
			}
			else if ( arg4 == 4 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not map the data buffer memory descriptor");  
			}
			else if ( arg4 == 5 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not get the virtual address of the map");  
			}
			else if ( arg4 == 6 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", 0, "Could not create a data buffer memory descriptor addr: 0x%x, size %d  status 0x%x", arg1, arg2, arg3 );  
			}
			else if ( arg4 == 7 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not prepare the data buffer memory descriptor status 0x%x", arg2 );  
			}
			else if ( arg4 == 8 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", 0, "Could not create a frame list memory descriptor addr: 0x%x, size %d status 0x%x", arg1, arg2, arg3 );  
			}
			else if ( arg4 == 9 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not prepare the frame list memory descriptor status 0x%x", arg1 );  
			}
			else if ( arg4 == 10 ) 
			{
				log(info, "InterfaceUserClient", "LL PrepareBuffer", parg1, "Could not map the frame list memory descriptor! status 0x%x", arg2 );  
			}
			break;

		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCChangeOutstandingIO ):
			if ( arg4 == 1 ) 
			{
				log(info, "InterfaceUserClient", "ChangeOutstandingIO", parg1, "invalid direction %d",  arg2 );
			}
			else
				log(info, "InterfaceUserClient", "ChangeOutstandingIO", parg1, "invalid target direction %d ", arg2 );
			break;
			
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCReqComplete ):
			log(info, "InterfaceUserClient", "ReqComplete", parg1, "Error: 0x%x, bytesTransferred: %d of %d", arg2, arg4 - arg3, arg4 );
			break;
			
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCIsoReqComplete ):
			log(info, "InterfaceUserClient", "IsoReqComplete", parg1, "Error: 0x%x, FrameListPtr: %p", arg2, (void*) arg3 );
			break;
			
		case USB_INTERFACE_UC_TRACE( kTPInterfaceUCLLIsoReqComplete ):
			log(info, "InterfaceUserClient", "LL IsoReqComplete", parg1, "Error: 0x%x, FrameListPtr: %p", arg2, (void*) arg3 );
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceEnumeration	( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_ENUMERATION_TRACE( kTPEnumerationProcessStatusChanged ):
			log(info, "Enumeration", "ProcessStatusChanged", parg1, "LocationID: 0x%x, Bitmap 0x%x", arg3, arg2 );
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationInitialGetPortStatus ):
			log(info, "Enumeration", "PortStatusChangeHandler", parg1, "Port %d:  Hub @ 0x%x, Status: 0x%4.04x, Change: 0x%4.04x", arg2, arg3, (arg4 & 0xFFFF0000)>>16, arg4  & 0xFFFF);
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationCallAddDevice ):
			log(info, "Enumeration", "DefaultConnectionChangeHandler", parg1, "Port %d of Hub at 0x%8.08x", arg2, arg3 );
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationAddDevice ):
			log(info, "Enumeration", "AddDevice", parg1, "Port %d of Hub at 0x%8.08x", arg2, arg3 );
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationResetPort ):
			log(info, "Enumeration", "AddDevice", parg1, "resetting Port %d of Hub at 0x%8.08x", arg2, arg3 );
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationAddDeviceResetChangeHandler ):
			log(info, "Enumeration", "AddDeviceResetChangeHandler", parg1, "Port %d of Hub at 0x%8.08x with error 0x%x", arg2, arg3, arg4 );
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationRegisterService ):
			log(info, "Enumeration", "AddDeviceResetChangeHandler", parg1, "Calling registerService for Port %d of Hub at 0x%8.08x", arg2, arg3 );
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationLowSpeedDevice ):
			log(info, "Enumeration", "EHCI Root Hub", parg1, "Found low speed device, giving it to companion");
			break;
			
		case USB_ENUMERATION_TRACE( kTPEnumerationFullSpeedDevice ):
			log(info, "Enumeration", "EHCI Root Hub", parg1, "Found full speed device, giving it to companion");
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}

#pragma mark UIM Tracepoints

static void 
CollectTraceUHCI ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_UHCI_TRACE( kTPUHCIMessage ):
			if ( arg4 == 1 ) 
			{
				log(info, "UHCI", "message", parg1, "got kIOUSBMessageExpressCardCantWake from driver 0x%x argument is 0x%x",  arg2, arg3 );
			}
			else
				log(info, "UHCI", "message", parg1, "device is attached to my root hub (port %d)!!", arg2 );
			break;
		
		case USB_UHCI_TRACE( kTPUHCIGetFrameNumber ):
			log(info, "UHCI", "message", parg1, "called but controller is halted powerState %d kUSBPowerStateOn",  arg2 );
			break;

		case USB_UHCI_TRACE( kTPUHCIScavengeIsocTransactions ):
			log(info, "UHCI", "message", parg1, "EP (0x%x) still had %d TDs on the reversed list!!",  arg2, arg3 );
			break;

		case USB_UHCI_TRACE( kTPUHCIScavengeQueueHeads ):
			if ( arg4 == 1 ) 
			{
				log(info, "UHCI", "message", 0, "releasing CBI buffer qTD->alignBuffer 0x%x direction %d actLen %d",  arg1, arg2, arg3 );
			}
			else if ( arg4 == 2 ) 
			{
				log(info, "UHCI", "message", 0, "IN transaction - storing UHCIAlignmentBuffer 0x%x into dmaCommand 0x%x to be copied later - actLegth %d",  arg1, arg2, arg3 );
			}
			else
				log(info, "UHCI", "message", parg1, "looks like bad ed queue (%d)",  arg2 );
			break;

		case USB_UHCI_TRACE( kTPUHCIAllocateQH ):
			if ( arg4 == 1 ) 
			{
				log(info, "UHCI", "AllocateQH", parg1, "hmm. ran out of EDs in a memory block i %d numQHs %d",  arg2, arg3 );
			}
			else 
				log(info, "UHCI", "AllocateQH", 0, "unable to allocate a new memory block! functionnumber %d endpoint %d direction %d type %d",  arg1, arg2, arg3, arg4 );
			break;

		case USB_UHCI_TRACE( kTPUHCIRootHubStatusChange ):
			log(info, "UHCI", "RH StatusChange", parg1, "Port Status Flags %d Port Change Flags %d Port %d attempting to enable a disabled port to work around a fickle UHCI controller",  arg2, arg3, arg4 );
			break;

		case USB_UHCI_TRACE( kTPUHCIRHSuspendPort ):
			log(info, "UHCI", "RH Suspend Port", parg1, "trying to suspend port (%d) which is being resumed - UNEXPECTED",  arg2 );
			break;

		case USB_UHCI_TRACE( kTPUHCIRHHoldPortReset ):
			log(info, "UHCI", "RH Hold Port Reset", parg1, "port (%d) ",  arg2 );
			break;

		case USB_UHCI_TRACE( kTPUHCIRHResumePortCompletion ):
			log(info, "UHCI", "RH Resume Port Completion", parg1, "port %d does not appear to be resuming! status 0x%x",  arg2, arg4 );
			break;
			
		case USB_UHCI_TRACE( KTPUHCIResumeController ):
			log(info, "UHCI", "ResumeController", parg1, NULL);
			break;
			
		case USB_UHCI_TRACE( kTPUHCISuspendController ):
			if ( arg4 == 1 ) 
			{
				log(info, "UHCI", "SuspendController", parg1, NULL);
			}
			else
			{
				log(info, "UHCI", "SuspendController", parg1, "port %d had the overcurrent bit set", arg2);
			}
			break;
			
		case USB_UHCI_TRACE( KTPUHCIResetControllerState ):
			log(info, "UHCI", "ResetControllerState", parg1, NULL);
			break;
			
		case USB_UHCI_TRACE( KTPUHCIRestartControllerFromReset ):
			log(info, "UHCI", "RestartControllerFromReset", parg1, NULL );
			break;
			
		case USB_UHCI_TRACE( KTPUHCIEnableInterrupts ):
			log(info, "UHCI", "EnableInterruptsFromController", parg1, "%s",  arg2 == 1 ? "enable": "disable");
			break;
			
		case USB_UHCI_TRACE( KTPUHCIDozeController ):
			log(info, "UHCI", "DozeController", parg1, NULL );
			break;
			
		case USB_UHCI_TRACE( KTPUHCIWakeFromDoze ):
			log(info, "UHCI", "WakeControllerFromDoze", parg1, NULL);
			break;
			
		case USB_UHCI_TRACE( KTPUHCIPowerState ):
			if ( arg4 == 1 ) 
			{
				log(info, "UHCI", "powerStateWillChangeTo", parg1, "new state: %d",  arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "UHCI", "powerStateDidChangeTo", parg1, "new state: %d",  arg2 );
			}
			else 
			{
				log(info, "UHCI", "powerStateDidChangeTo", parg1, "from state (%d) to state (%d)",  arg2, arg3);
			}
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceUHCIUIM	( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_UHCI_UIM_TRACE( kTPUHCIUIMCreateControlEndpoint ):
			log(info, "UHCIUIM", "CreateControlEndpoint", 0, "maxPacketSize 0 is illegal (kIOReturnBadArgument) function %d endpoint %d speed %d status 0x%x ",  arg1, arg2, arg3, arg4 );
			break;

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMCreateBulkEndpoint ):
			log(info, "UHCIUIM", "CreateBulkEndpoint", 0, "maxPacketSize 0 is illegal (kIOReturnBadArgument) function %d endpoint %d speed %d status 0x%x ",  arg1, arg2, arg3, arg4 );
			break;
			
		case USB_UHCI_UIM_TRACE( kTPUHCIUIMCreateInterruptEndpoint ):
			if (arg4 == kIOReturnBadArgument)
			{
				log(info, "UHCIUIM", "CreateInterruptEndpoint", 0, "maxPacketSize 0 is illegal (kIOReturnBadArgument) function %d endpoint %d direction %d status 0x%x ",  arg1, arg2, arg3, arg4 );
			}
			else
				log(info, "UHCIUIM", "CreateInterruptEndpoint", 0, "deleting endpoint function %d endpoint %d direction %d returned 0x%x ",  arg1, arg2, arg3, arg4 );
			break;
		
		case USB_UHCI_UIM_TRACE( kTPUHCIUIMCreateInterruptTransfer ):
			if (arg4 == kIOUSBEndpointNotFound)
			{
				log(info, "UHCIUIM", "CreateInterruptTransfer", 0, "QH not found address 0x%x endpoint %d direction %d status 0x%x ",  arg1, arg2, arg3, arg4 );
			}
			else
				log(info, "UHCIUIM", "CreateInterruptTransfer", 0, "Interrupt pipe stalled address 0x%x endpoint %d direction %d status 0x%x ",  arg1, arg2, arg3, arg4 );
			break;

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMCreateIsochEndpoint ):
			if (arg4 == 1)
			{
				log(info, "UHCIUIM", "CreateIsochEP", 0, "out of bandwidth, request (extra) = %d, available: %d status 0x%x ",  arg1, arg2, arg3 );
			}
			else
				log(info, "UHCIUIM", "CreateIsochEP", 0, "out of bandwidth, request (extra) = %d, available: %d status 0x%x ",  arg1, arg2, arg3 );
			break;

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMCreateIsochTransfer ):
			if (arg4 == 1)
			{
				log(info, "UHCIUIM", "CreateIsochTransfer", 0, "Isoch frame (%d) too big (%d) MPS (%d)", arg1, arg2, arg3 );
			}
			else if (arg4 == 2)
			{
				log(info, "UHCIUIM", "CreateIsochTransfer", 0, "LL Isoch frame (%d) too big (%d) MPS (%d)", arg1, arg2, arg3 );
			}
			else if (arg4 == 3)
			{
				log(info, "UHCIUIM", "CreateIsochTransfer", parg1, "Could not allocate a new iTD 0x%x reqCount %d", arg2, arg3 );
			}
			else if (arg4 == 4)
			{
				log(info, "UHCIUIM", "CreateIsochTransfer", parg1, "could not get the alignment buffer I needed address 0x%x endpoint %d", arg2, arg3 );
			}
			else if (arg4 == 5)
				log(info, "UHCIUIM", "CreateIsochTransfer", parg1, "Endpoint not found address 0x%x endpoint %d", arg2, arg3 );
			break;

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMHandleEndpointAbort ):
			if ( arg4 == kIOReturnBadArgument )
			{
				log(info, "UHCIUIM", "EndpointAbort", parg1, "bad params - endpNumber: %d direction %d status 0x%x", arg2, arg3, arg4 );
			}
			else if ( arg4 == kIOUSBEndpointNotFound )
			{
				log(info, "UHCIUIM", "EndpointAbort", parg1, "endpoint not found - endpNumber: %d direction %d status 0x%x", arg2, arg3, arg4 );
			}
			break;

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMDeleteEndpoint ):
			if ( arg4 == kIOReturnBadArgument )
			{
				log(info, "UHCIUIM", "DeleteEndpoint", parg1, "not ep 0 or ep 1 - endpNumber: %d direction %d status 0x%x", arg2, arg3, arg4 );
			}
			else if ( arg4 == kIOUSBEndpointNotFound )
			{
				log(info, "UHCIUIM", "DeleteEndpoint", parg1, "endpoint not found - endpNumber: %d direction %d status 0x%x", arg2, arg3, arg4 );
			}
			else
				log(info, "UHCIUIM", "DeleteEndpoint", parg1, "unlinking endpoint - status 0x%x", arg2);
			break;
		
		case USB_UHCI_UIM_TRACE( kTPUHCIUIMUnlinkQueueHead ):
			log(info, "UHCIUIM", "UnlinkQueueHead", parg1, "invalid params pQH 0x%x pQHBack 0x%x - status 0x%x", arg2, arg3, arg4 );
			break;
			
		case USB_UHCI_UIM_TRACE( kTPUHCIUIMCheckForTimeouts ):
			if ( arg4 == 1 )
			{
				log(info, "UHCIUIM", "UnlinkQueueHead", parg1, "ED 0x%x - TD is TAIL but there is a command - pTD 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "UHCIUIM", "UnlinkQueueHead", parg1, "Too many loops around - ", arg2 );
			}
			break;
		

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMReturnOneTransaction ):
			log(info, "UHCIUIM", "Return1Transaction", parg1, "got to the end with no callback pTD 0x%x ", arg2 );
			break;
		
		case USB_UHCI_UIM_TRACE( kTPUHCIUIMAllocateTDChain ):
			if ( arg4 == 1 )
			{
				log(info, "UHCIUIM", "AllocateTDChain", parg1, "pTD 0x0x%x using UHCIAlignmentBuffer 0x0x%x ", arg2, arg3);
			}
			else if ( arg4 == 2 )
			{
				log(info, "UHCIUIM", "AllocateTDChain", parg1, "paddr 0x0x%x instead of CBP 0x0x%x ", arg2, arg3);
			}
			else if ( arg4 == 3 )
			{
				log(info, "UHCIUIM", "AllocateTDChain", parg1, "moving alignBuffer from TD 0x0x%x to TD 0x0x%x ", arg2, arg3 );
			}
			else
				log(info, "UHCIUIM", "AllocateTDChain", parg1, "dmaStartAddr 0x0x%x totalPhysLength %d bytesToSchedule %d ", arg2, arg3, arg4 );
			break;

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMAddIsochFramesToSchedule ):
			if ( arg4 == 1 )
			{
				log(info, "UHCIUIM", "AddIsochFramesToSchedule", parg1, "EP is aborting - not adding function %d endpoint %d ", arg2, arg3);
			}
			else if ( arg4 == 2 )
			{
				log(info, "UHCIUIM", "AddIsochFramesToSchedule", parg1, "Current frame moved (0x%x->0x%x) resetting ", arg2, arg3);
			}
			break;

		case USB_UHCI_UIM_TRACE( kTPUHCIUIMAbortIsochEP ):
			if ( arg4 == 1 )
			{
				log(info, "UHCIUIM", "AbortIsochEP", parg1, "err (0x%x) from scavengeIsochTransactions ", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "UHCIUIM", "AbortIsochEP", parg1, "NULL endpoint in  pEP 0x%x pTD 0x%x ", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "UHCIUIM", "AbortIsochEP", parg1, "pEP 0x%x _outSlot 0x%x  ", arg2, arg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "UHCIUIM", "AbortIsochEP", 0, "pEP->inSlot 0x%x activeTDs %d onToDoList %d  ", arg1, arg2, arg3 );
			}
			else if ( arg4 == 5 )
			{
				log(info, "UHCIUIM", "AbortIsochEP", 0, "todo 0x%x deferredTDs %d deferred 0x%x", arg1, arg2, arg3 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "UHCIUIM", "AbortIsochEP", 0, "scheduledTDs (%d) onProducerQ (%d) consumer (%d) ", arg1, arg2, arg3 );
			}
			else
				log(info, "UHCIUIM", "AbortIsochEP", 0, "producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (0x%x) ", arg1, arg2, arg3, arg4 );
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceUHCIInterrupts ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_UHCI_INTERRUPTS_TRACE( kTPUHCIInterruptsGetFrameNumberInternal ):
			log(info, "UHCI", "GetFrameNumber", parg1, "called but controller is halted");
			break;
		
		case USB_UHCI_INTERRUPTS_TRACE( kTPUHCIInterruptsFilterInterrupt ):
			if ( arg4 == 1 )
			{
				log(info, "UHCI", "FilterInterrupt", parg1, "_hostControllerProcessInterrupt %d HCPE error - legacy reg 0x%x ", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "UHCI", "FilterInterrupt", parg1, "_hostSystemErrorInterrupt %d HSE error  - legacy reg 0x%x ", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "UHCI", "FilterInterrupt", parg1, "active interrupts 0x%4.04x", arg2 );
			}
			break;

		case USB_UHCI_INTERRUPTS_TRACE( kTPUHCIInterruptsHandleInterrupt ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "UHCI", "Begin HandleInterrupt", parg1, NULL);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "UHCI", "End HandleInterrupt", parg1, NULL );
			} 
			else 
			{
				if ( arg4 == 1 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "Host controller process error");
				}
				else if ( arg4 == 2 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "Host controller system error  CMD: 0x%x ", arg2 );
				}
				else if ( arg4 == 3 )
				{
					log(info, "UHCI", "HandleInterrupt", 0, "Host controller system error  STS:0x%x INTR:0x%x PORTSC1:0x%x ", arg1, arg2, arg3 );
				}
				else if ( arg4 == 4 )
				{
					log(info, "UHCI", "HandleInterrupt", 0, "Host controller system error  PORTSC2:0x%x FRBASEADDR:0x%x ConfigCMD:0x%x ", arg1, arg2, arg3 );
				}
				else if ( arg4 == 5 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "Resetting controller due to errors detected at interrupt time status 0x%x needReset %d ", arg2, arg3 );
				}
				else if ( arg4 == 6 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "ResumeDetected ");
				}
				else if ( arg4 == 7 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "Host controller error ");
				}
				else if ( arg4 == 8 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "Normal Interrupt ");
				}
				else if ( arg4 == 9 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "Processing interrupt USBSTS = 0x%4.04x", arg2 );
				}
				else if ( arg4 == 10 )
				{
					log(info, "UHCI", "HandleInterrupt", parg1, "deferring further processing until we are running again");
				}
			}
			break;

		case USB_UHCI_INTERRUPTS_TRACE( kTPUHCIUpdateFrameList ):
#ifdef __LP64__
			log(info, "UHCI", "UpdateFrameList", 0, "Address: %d, Endpoint: %d, (%s), frameListPtr: 0x%qx, frActCount: 0x%x, timeStamp: 0x%qx", 
				((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), ((arg1 >> 24) & 0xFF) == kUSBIn ? "in" : "out",
				(uint64_t)arg2, arg3, (uint64_t)arg4 );
#else
			log(info, "UHCI", "UpdateFrameList", 0, "Address: %d, Endpoint: %d, (%s), frameListPtr: 0x%x, timeStamp: 0x%x 0x%x", 
				((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), ((arg1 >> 24) & 0xFF) == kUSBIn ? "in" : "out",
				arg2, arg3, arg4 );
#endif
			break;
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceOHCI ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_OHCI_TRACE( kTPOHCIInitialize ):
			log(info, "OHCI", "Init", parg1, "HCCA Buffer pPhysical 0x%x logical 0x%x ", arg2, arg3 );
			break;

		case USB_OHCI_TRACE( kTPOHCIAllocateITD ):
			if ( arg4 == 1 )
			{
				log(info, "OHCI", "AllocateITD", parg1, "unable to allocate a new memory block!");
			}
			else if ( arg4 == 2 )
			{
				log(info, "OHCI", "AllocateITD", parg1, "hmm. ran out of TDs in a memory block i %d numTDs %d ", arg2, arg3 );
			}
			break;
			
		case USB_OHCI_TRACE( kTPOHCIAllocateTD ):
			if ( arg4 == 1 )
			{
				log(info, "OHCI", "AllocateTD", parg1, "unable to allocate a new memory block!");
			}
			else if ( arg4 == 2 )
			{
				log(info, "OHCI", "AllocateTD", parg1, "hmm. ran out of TDs in a memory block ");
			}
			break;
	
		case USB_OHCI_TRACE( kTPOHCIAllocateED ):
			if ( arg4 == 1 )
			{
				log(info, "OHCI", "AllocateED", parg1, "unable to allocate a new memory block!");
			}
			else if ( arg4 == 2 )
			{
				log(info, "OHCI", "AllocateED", parg1, "hmm. ran out of TDs in a memory block ");
			}
			break;
		
		case USB_OHCI_TRACE( kTPOHCIProcessCompletedITD ):
			log(info, "OHCI", "ProcessCompletedITD", parg1, "_activeIsochTransfers went negative (%d).  We lost one somewhere ", arg2 );
			break;
		
		case USB_OHCI_TRACE( kTPOHCIReturnTransactions ):
			if ( arg4 == 1 )
			{
				log(info, "OHCI", "ReturnTransactions", parg1, "Isoc Return queue broken");
			}
			else if ( arg4 == 2 )
			{
				log(info, "OHCI", "ReturnTransactions", parg1, "Return queue broken ");
			}
			break;

		case USB_OHCI_TRACE( kTPOHCIMessage ):
			log(info, "OHCI", "Message", parg1, "got kIOUSBMessageExpressCardCantWake");
			break;

		case USB_OHCI_TRACE( kTPOHCICreateGeneralTransfer ):
			if (arg3 == kIOUSBPipeStalled)
			{
				log(info, "OHCI", "CreateGeneralTransfer", parg1, "trying to queue to a stalled pipe type %d status 0x%x", arg2, arg3 );
			}
			else
				log(info, "OHCI", "CreateGeneralTransfer", parg1, "returning status 0x%x", arg2 );
			break;

		case USB_OHCI_TRACE( kTPOHCIAbortEndpoint ):
			log(info, "OHCI", "AbortEndpoint", parg1, "bad params - function 0x%x endpNumber: %d status 0x%x", arg2, arg3, arg4 );
			break;

		case USB_OHCI_TRACE( kTPOHCIDeleteEndpoint ):
			log(info, "OHCI", "DeleteEndpoint", parg1, "bad params - function 0x%x endpNumber: %d status 0x%x", arg2, arg3, arg4 );
			break;
		
		case USB_OHCI_TRACE( kTPOHCIEndpointStall ):
			log(info, "OHCI", "ClearEndpointStall", parg1, "bad params - function 0x%x endpNumber: %d status 0x%x", arg2, arg3, arg4 );
			break;

		case USB_OHCI_TRACE( kTPOHCICreateIsochTransfer ):
			log(info, "OHCI", "CreateIsochTransfer", 0, "Could not allocate a new iTD bufferSize %d frameCount %d updateFrequency %d status 0x%x", arg1, arg2, arg3, arg4 );
			break;
			
		case USB_OHCI_TRACE( KTPOHCISuspendUSBBus ):
			log(info, "OHCI", "SuspendUSBBus", parg1, "going to sleep: %s", arg2 == 1 ? "yes": "no");
			break;
			
		case USB_OHCI_TRACE( KTPOHCIResumeUSBBus ):
			log(info, "OHCI", "ResumeUSBBus", parg1, "waking from sleep: %s", arg2 == 1 ? "yes": "no");
			break;
			
		case USB_OHCI_TRACE( KTPOHCIResetControllerState ):
			log(info, "OHCI", "ResetControllerState", parg1, NULL );
			break;
			
		case USB_OHCI_TRACE( KTPOHCIRestartControllerFromReset ):
			log(info, "OHCI", "RestartControllerFromReset", parg1, NULL );
			break;
			
		case USB_OHCI_TRACE( KTPOHCIEnableInterrupts ):
			log(info, "OHCI", "EnableInterruptsFromController", parg1,"%s", arg2 == 1 ? "enable": "disable");
			break;
			
		case USB_OHCI_TRACE( KTPOHCIDozeController ):
			log(info, "OHCI", "DozeController", parg1, NULL );
			break;
			
		case USB_OHCI_TRACE( KTPOHCIWakeControllerFromDoze ):
			log(info, "OHCI", "WakeControllerFromDoze", parg1, NULL );
			break;
			
		case USB_OHCI_TRACE( KTPOHCIPowerState ):
			log(info, "OHCI", "powerChangeDone", parg1, "from state (%d) to state (%d)", arg2, arg3);
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}

static void 
CollectTraceOHCIInterrupts	( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{	
		case USB_OHCI_INTERRUPTS_TRACE( kTPOHCIInterruptsPollInterrupts ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "OHCI", "Begin PollInterrupts", parg1, NULL);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "OHCI", "End PollInterrupts", parg1, NULL );
			} 
			else 
			{
				if ( arg4 == 1 )
				{					
					log(info, "OHCI", "PollInterrupts", parg1, "WriteDoneHead" );
				}
				else if ( arg4 == 2 )
				{					
					log(info, "OHCI", "PollInterrupts", parg1, "ResumeDetected" );
				}
				else if ( arg4 == 3 )
				{					
					log(info, "OHCI", "PollInterrupts", parg1, "Unrecoverable error  %d", arg2);
				}
				else if ( arg4 == 4 )
				{					
					log(info, "OHCI", "PollInterrupts ", parg1, "RootHubStatusChange");
				}
				else if ( arg4 == 5 )
				{					
					log(info, "OHCI", "PollInterrupts", parg1, "FrameNumberOverflow");
				}
			}
			break;
			
		case USB_OHCI_INTERRUPTS_TRACE( kTPOHCIInterruptsPrimaryInterruptFilter ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "OHCI", "Begin Primary Interrupt", parg1, NULL);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "OHCI", "End Primary Interrupt", parg1, "status 0x%x", arg2 );
			} 
			else
			{
				log(info, "OHCI", "Primary Interrupt", parg1, "enabledInterrupts 0x%8.08x activeInterrupts 0x%8.08x", arg2, arg3 );
			}
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}

static void 
CollectTraceEHCI ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_EHCI_TRACE( kTPEHCIUIMFinalize ):
			log(info, "EHCI", "UIMFinalize", parg1, "isInactive(%x) _pEHCIRegisters(0x%x) _device(0x%x)",  arg2, arg3, arg4 );
			break;


		case USB_EHCI_TRACE( kTPEHCIGetFrameNumber32 ):
			log(info, "EHCI", "GetFrameNumber32", parg1, "called but controller is halted STS: %x ",  arg2 );
			break;
		
		case USB_EHCI_TRACE( kTPEHCIAllocateQH ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "AllocateED", parg1, "unable to allocate a new memory block!",  arg2, arg3 );    
			}
			else if ( arg4 == 2 )
				log(info, "EHCI", "AllocateED", parg1, "hmm. ran out of EDs in a memory block");
	        break;

		case USB_EHCI_TRACE( kTPEHCIEnableAsyncSchedule ):
			log(info, "EHCI", "EnableAsyncSchedule", parg1, "returning status %x",  arg2 );
			break;
        
		case USB_EHCI_TRACE( kTPEHCIDisableAsyncSchedule ):
			if ( arg4 == kIOReturnInternalError )
			{
				log(info, "EHCI", "DisableAsyncSchedule", parg1, "ERROR: USBCMD (%x) and USBSTS (%x) won't synchronize OFF",  arg2, arg3, arg4 );
			}
			else
				log(info, "EHCI", "DisableAsyncSchedule", parg1, "returning status %x",  arg2);	
			break;


		case USB_EHCI_TRACE( kTPEHCIEnablePeriodicSchedule ):
			if ( arg4 == kIOReturnInternalError )
			{
				log(info, "EHCI", "EnablePeriodicSchedule", parg1, "ERROR: USBCMD (%x) and USBSTS (%x) won't synchronize OFF",  arg2, arg3, arg4 );
			}
			else
				log(info, "EHCI", "EnablePeriodicSchedule", parg1, "returning status %x",  arg2);	
			break;


		case USB_EHCI_TRACE( kTPEHCIDisablePeriodicSchedule ):
			if ( arg4 == kIOReturnInternalError )
			{
				log(info, "EHCI", "DisablePeriodicSchedule", parg1, "ERROR: USBCMD (%x) and USBSTS (%x) won't synchronize OFF",  arg2, arg3, arg4 );
			}
			else
				log(info, "EHCI", "DisablePeriodicSchedule", parg1, "returning status %x",  arg2);	
			break;

		case USB_EHCI_TRACE( kTPEHCIMessage ):
			if ( arg4 == kIOUSBMessageExpressCardCantWake )
			{
				log(info, "EHCI", "Message", parg1, "got kIOUSBMessageExpressCardCantWake from driver [0x%x] nub is [0x%x]", arg2, arg3 );
			}
			else
				log(info, "EHCI", "Message", parg1, "device is attached to my root hub!!");
			break;
			
		case USB_EHCI_TRACE( kTPEHCISuspendUSBBus ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "EHCI", "Begin SuspendUSBBus", parg1, NULL );
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "EHCI", "End SuspendUSBBus", parg1, NULL );
			} 
			else 
			{
				log(info, "EHCI", "SuspendUSBBus", parg1, "suspendUSBBus - HC not halting! USBCMD(0x%x) USBSTS(0x%x) i(%d)", arg2, arg3, arg4 );
			}
			break;

		case USB_EHCI_TRACE( kTPEHCIRootHubPortSuspend ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "EHCIRootHubPortSuspend", parg1, "trying to suspend port (%d) which is being resumed - UNEXPECTED suspend %d", arg2, arg3 );
			}
			else
				log(info, "EHCI", "EHCIRootHubPortSuspend", parg1, "resuming port %d, but callout thread is NULL", arg2 );
			break;

		case USB_EHCI_TRACE( kTPEHCIMakeEmptyEndPoint ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "MakeEmptyEndPoint", parg1, "new endpoint NOT fixing up speed functionAddress %d endpointNumber %d speed %d", arg2, arg3, arg4 );
			}
			else
				log(info, "EHCI", "MakeEmptyEndPoint", parg1, "old endpoint found, abusing functionAddress %d endpointNumber %d speed %d", arg2, arg3, arg4);
			break;

		case USB_EHCI_TRACE( kTPEHCIAllocateTDs ):
			log(info, "EHCI", "AllocateTDs", parg1, "maxPacket for control endpoint (%d) was 0! - returning kIOReturnNotPermitted", arg2 );
			break;

		case USB_EHCI_TRACE( kTPEHCIMungeECHIStatus ):
			log(info, "EHCI", "MungeECHIStatus", parg1, "condition we're not expecting 0x%x kOHCIITDConditionCRC", arg2 );
			break;
		
		case USB_EHCI_TRACE( kTPEHCIScavengeIsocTransactions ):
			log(info, "EHCI", "scavengeIsocTransactions", parg1, "EP (0x%x) still had %d TDs on the reversed list!!", arg2, arg3 );
			break;

		case USB_EHCI_TRACE( kTPEHCIScavengeAnEndpointQueue ):
			log(info, "EHCI", "scavengeAnEndpointQueue", parg1, "looks like bad ed queue %x", arg2 );
			break;

		case USB_EHCI_TRACE( kTPEHCIScavengeCompletedTransactions ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "scavengeCompletedTransactions", parg1, "err isoch list %x", arg2 );
			}
			else if (arg4 == 2)
			{
				log(info, "EHCI", "scavengeAnEndpointQueue", parg1, "err async queue %x", arg2 );
			}
			else if (arg4 == 3)
				log(info, "EHCI", "scavengeCompletedTransactions", parg1, "periodic queue[%d] err %x", arg2, arg3 );
			break;

		case USB_EHCI_TRACE( kTPEHCICreateBulkEndpoint ):
			log(info, "EHCI", "UIMCreateBulkEndpoint", parg1, "kIOReturnBadArgument functionAddress %d endpointNumber %d direction %d", arg2, arg3, arg4 );
			break;
		
		case USB_EHCI_TRACE( kTPEHCICreateBulkTransfer ):
			log(info, "EHCI", "UIMCreateBulkTransfer", parg1, "allocateTDs returned error", arg2 );
			break;
			
		case USB_EHCI_TRACE( kTPEHCICreateInterruptEndpoint ):
			log(info, "EHCI", "UIMCreateInterruptEndpoint", parg1, "functionAddress %d endpointNumber %d  rawPollingRate (%d)", arg2, arg3, arg4 );
			break;

	
		case USB_EHCI_TRACE( kTPEHCICreateIsochEndpoint ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "UIMCreateIsochEndpoint", parg1, "kIOReturnNoBandwidth out of bandwidth 1, request (extra) = %d, available: %d", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "UIMCreateIsochEndpoint", parg1, "kIOReturnNoBandwidth out of bandwidth 2, request (extra) = %d, available: %d", arg2, arg3 );
			}
			break;

		case USB_EHCI_TRACE( kTPEHCIAbortIsochEP ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "AbortIsochEP", parg1, "err (0x%x) from scavengeIsocTransactions", arg2 ) ;
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "AbortIsochEP", parg1, "scheduleTDs is negative!", arg2, arg3 ) ;
			}
			else if ( arg4 == 3 )
			{
				log(info, "EHCI", "AbortIsochEP", parg1, "NULL endpoint in pTD 0x%x", arg2, arg3 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "EHCI", "AbortIsochEP", 0, "done _endpoint 0x%x outSlot (0x%x) pEP->inSlot (0x%x)", arg1, arg2, arg3);
			}
			else if ( arg4 == 5 )
			{
				log(info, "EHCI", "AbortIsochEP", 0, "activeTDs (%d) onToDoList (%d) todo (0x%x)",  arg1, arg2, arg3 );
			}
			else if ( arg4 == 6 )
			{
				log(info, "EHCI", "AbortIsochEP", 0, "deferredTDs (%d) deferred(0x%x) scheduledTDs (%d)",  arg1, arg2, arg3 );
			}
			else if ( arg4 == 7 )
			{
				log(info, "EHCI", "AbortIsochEP", 0, "onProducerQ (%d) consumer (%d) producer (%d)",  arg1, arg2, arg3 );
			}
			else if ( arg4 == 8 )
			{
				log(info, "EHCI", "AbortIsochEP", 0, "onReversedList (%d) onDoneQueue (%d)  doneQueue (0x%x)",  arg1, arg2, arg3 );
			}
			break;

		case USB_EHCI_TRACE( kTPEHCIHandleEndpointAbort ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "HandleEndpointAbort", 0, "ReEntered!!  functionAddress %d endpointNumber %d direction %d", arg1, arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "HandleEndpointAbort", 0, "kIOReturnBadArgument functionAddress %d endpointNumber %d ", arg1, arg2 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "EHCI", "HandleEndpointAbort", 0, "kIOUSBEndpointNotFound functionAddress %d endpointNumber %d ", arg1, arg2 );
			}
			else if ( arg4 == 4 )
			{
				log(info, "EHCI", "HandleEndpointAbort", 0, "QH still halted following returnTransactions!! ");
			}
			break;
        
		case USB_EHCI_TRACE( kTPEHCICreateInterruptTransfer ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "UIMCreateInterruptTransfer", parg1, "kIOUSBEndpointNotFound endpoint %d ", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "UIMCreateInterruptTransfer", parg1, "address %x allocateTDs failed status %x", arg2, arg3 );
			}
			break;

		case USB_EHCI_TRACE( kTPEHCIUnlinkAsyncEndpoint ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "unlinkAsyncEndpoint", parg1, "the schedule status didn't go OFF in the STS register %x !! ", arg2 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "unlinkAsyncEndpoint", parg1, "the schedule status didn't go ON in the STS register %x !! ", arg2 );
			}
			break;
		
		case USB_EHCI_TRACE( kTPEHCIDeleteEndpoint ):
			log(info, "EHCI", "UIMDeleteEndpoint", 0, "bad params kIOReturnBadArgument - functionAddress %d endpointNumber %d direction %d", arg1, arg2, arg3 );
			break;

		case USB_EHCI_TRACE( kTPEHCICreateHSIsochTransfer ):
			log(info, "EHCI", "CreateHSIsochTransfer", parg1, "Could not allocate a new iTD status = %x", arg2 );
			break;

		case USB_EHCI_TRACE( kTPEHCICreateSplitIsochTransfer ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "CreateSplitIsochTransfer", 0, "(LL) Isoch frame (%d) too big (%d) MPS (%d)", arg1, arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "CreateSplitIsochTransfer", 0, "Isoch frame (%d) too big (%d) MPS (%d)", arg1, arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "EHCI", "CreateSplitIsochTransfer", parg1, "Could not allocate a new iTD status = %x", arg3 );
			}
			break;

		case USB_EHCI_TRACE( kTPEHCICreateIsochTransfer ):
			log(info, "EHCI", "UIMCreateIsochTransfer", parg1, "Endpoint not found status = %x", arg4 );
			break;
		

		case USB_EHCI_TRACE( kTPEHCIAddIsocFramesToSchedule ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "AddIsocFramesToSchedule", parg1, "EP is aborting - not adding functionAddress %d endpointNumber %d", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "AddIsocFramesToSchedule", parg1, "thread_call_enter1(_returnIsochDoneQueueThread) was NOT scheduled.  That's not good");
			}
			break;

		case USB_EHCI_TRACE( kTPEHCIReturnOneTransaction ):
			log(info, "EHCI", "ReturnOneTransaction", parg1, "returning all TDs on the queue status = %x", arg2 );
			break;

		case USB_EHCI_TRACE( kTPEHCICheckEDListForTimeouts ):
			log(info, "EHCI", "CheckEDListForTimeouts", parg1, "ED (0x%x) - TD is TAIL but there is a command - pTD (0x%x)", arg2, arg3 );
			break;

		case USB_EHCI_TRACE( kTPEHCICheckForTimeouts ):
			log(info, "EHCI", "UIMCheckForTimeouts", parg1, "aborting check with outstanding transactions! _myBusState(%d) _wakingFromHibernation(%d)", arg2, arg3 );
			break;

		case USB_EHCI_TRACE( kTPEHCIRootHubResetPort ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "RootHubResetPort", parg1, "Not resetting port, because device is unplugged or powered off - port %d value %d", arg2, arg3 );
			}
			else if ( arg4 == 2 )
			{
				log(info, "EHCI", "RootHubResetPort", parg1, "portSC is not equal to value of register! (0x%x)(0x%x) ", arg2, arg3 );
			}
			else if ( arg4 == 3 )
			{
				log(info, "EHCI", "RootHubResetPort", parg1, "Not resetting port 2, because device is unplugged or powered off - port %d value %d", arg2, arg3 );
			}
			break;

    	case USB_EHCI_TRACE( kTPEHCIRootHubPortEnable ):
			log(info, "EHCI", "RootHubPortEnable", parg1, "enabling port %d enable %d illegal kIOReturnUnsupported", arg2, arg3 );
			break;

    	case USB_EHCI_TRACE( kTPEHCIRHResumePortCompletion ):
			log(info, "EHCI", "RHResumePortCompletion", parg1, "port %d does not appear to be resuming! status = %x", arg2, arg3 );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIResumeUSBBus ):
			log(info, "EHCI", "ResumeUSBBus", parg1, NULL );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIRestartUSBBus ):
			log(info, "EHCI", "RestartUSBBus", parg1, NULL );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIStopUSBBus ):
			log(info, "EHCI", "StopUSBBus", parg1, NULL );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIResetControllerState ):
			log(info, "EHCI", "ResetControllerState", parg1, NULL );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIRestartControllerFromReset ):
			log(info, "EHCI", "RestartControllerFromReset", parg1, NULL );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIEnableInterrupts ):
			log(info, "EHCI", "EnableInterruptsFromController", parg1,"%s", arg2 == 1 ? "enable": "disable");
			break;
			
		case USB_EHCI_TRACE( kTPEHCIDozeController ):
			log(info, "EHCI", "DozeController", parg1, NULL );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIWakeControllerFromDoze ):
			log(info, "EHCI", "WakeControllerFromDoze", parg1, NULL );
			break;
			
		case USB_EHCI_TRACE( kTPEHCIPowerState ):
			if ( arg4 == 1 )
			{
				log(info, "EHCI", "powerChangeDone", parg1, "from state (%d) to state (%d)", arg2, arg3);
			}
			else
			{
				log(info, "EHCI", "powerStateDidChangeTo", parg1, "stateNumber(%d)", arg2);
		}
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}

static void 
CollectTraceEHCIHubInfo	( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
    	case USB_EHCI_HUBINFO_TRACE( kTPEHCIAvailableIsochBandwidth ):
			log(info, "EHCIHubInfo", "AvailableIsochBandwidth", parg1, "invalid direction %d", arg2 );
			break;

    	case USB_EHCI_HUBINFO_TRACE( kTPEHCIAllocateIsochBandwidth ):
			if ( arg4 == 2 )
			{			
				log(info, "EHCIHubInfo", "AvailableIsochBandwidth", 0, "returning kIOReturnNoBandwidth maxPacketSize %d remainder %d", arg1, arg2, arg3 );
			}
			else if ( arg4 == 1 )
			{			
				log(info, "EHCIHubInfo", "AllocateIsochBandwidth", parg1, "unknown pEP direction direction (%d) maxPacketSize %d ", arg2, arg3 );
			}
			else
			{
				if ( parg1 == kUSBIn && arg4 == kIOReturnNoBandwidth )
				{				
					log(info, "EHCIHubInfo", "AllocateIsochBandwidth", parg1, "not enough bandwidth for IN transaction remainder %d, maxPacketSize %d ", arg2, arg3 );
				}
				else if (parg1 == kUSBOut)
				{
				    log(info, "EHCIHubInfo", "AllocateIsochBandwidth", parg1, "not enough bandwidth for OUT transaction  remainder %d, maxPacketSize %d",  arg2, arg3 );
				}
				else if (parg1 == kUSBIn)
				{
					log(info, "EHCIHubInfo", "AllocateIsochBandwidth", parg1, "ran out of IN frames - need to redo remainder %d, maxPacketSize %d ", arg2, arg3 );
				}
			}
			break;
		
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}
	

static void 
CollectTraceEHCIInterrupts	( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{	
		case USB_EHCI_INTERRUPTS_TRACE( kTPEHCIInterruptsPollInterrupts ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "EHCI", "Begin PollInterrupts", parg1, "_errorInterrupt %x _completeInterrupt %x _portChangeInterrupt %x", arg2, arg3, arg4);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "EHCI", "End PollInterrupts", parg1, NULL );
			} 
			else 
			{
				if ( arg4 == 1 )
				{					
					log(info, "EHCI", "PollInterrupts", parg1, "Host System Error Occurred - not restarted errors.displayed %d",  arg2 );
				}
				else if ( arg4 == 2 )
				{					
					log(info, "EHCI", "PollInterrupts", parg1, "port %d appears to be resuming from a remote wakeup, but the thread callout is NULL!",  arg2 );
				}
				else if ( arg4 == 3 )
				{					
					log(info, "EHCI", "PollInterrupts", parg1, "Completion Interrupt");
				}
				else if ( arg4 == 4 )
				{					
					log(info, "EHCI", "PollInterrupts", parg1, "Error Interrupt");
				}
				else if ( arg4 == 5 )
				{					
					log(info, "EHCI", "PollInterrupts", parg1, "Port Change Interrupt");
				}
				else if ( arg4 == 6 )
				{					
					log(info, "EHCI", "PollInterrupts", parg1, "Async Advance Interrupt");
				}
				else if ( arg4 == 7 )
				{					
					log(info, "EHCI", "PollInterrupts", parg1, "Frame Rollover Interrupt");
				}
			}
			break;

		case USB_EHCI_INTERRUPTS_TRACE( kTPEHCIInterruptsPrimaryInterruptFilter ):
			if ( qualifier == DBG_FUNC_START ) 
			{
	        	log(info, "EHCI", "Begin Primary Interrupt", parg1, NULL);
			} 
			else if ( qualifier == DBG_FUNC_END ) 
			{
				log(info, "EHCI", "End Primary Interrupt", parg1, "status 0x%x", arg2 );
			} 
			else
			{
				log(info, "EHCI", "Primary Interrupt", parg1, "enabledInterrupts 0x%8.08x activeInterrupts 0x%8.08x, Frame: %d, microframe: %d", arg2, arg3, (arg4 >> 3), (arg4 && 0x7) );
			}
			break;
			
		case USB_EHCI_INTERRUPTS_TRACE( kTPEHCIUpdateFrameList ):
#ifdef __LP64__
			log(info, "UHCI", "UpdateFrameList", 0, "Address: %d, Endpoint: %d, (%s), frameListPtr: 0x%qx, frActCount: 0x%x, timeStamp: 0x%qx", 
				((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), ((arg1 >> 24) & 0xFF) == kUSBIn ? "in" : "out",
				(uint64_t)arg2, arg3, (uint64_t)arg4 );
#else
			log(info, "EHCI", "UpdateFrameList", 0, "Address: %d, Endpoint: %d, (%s), frameListPtr: 0x%x, timeStamp: 0x%x 0x%x ( %qd )", 
				((arg1 >> 8) & 0xFF), ((arg1 >> 0) & 0xFF), ((arg1 >> 24) & 0xFF) == kUSBIn ? "in" : "out",
				arg2, arg3, arg4, (uint64_t) arg4 );
#endif
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceHubPolicyMaker	( kd_buf tracepoint )
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_HUB_POLICYMAKER_TRACE( kTPSetPowerState ):
			if ( arg4 == kIOPMNoSuchState )
			{
				log( info, "HubPolicyMaker", "setpowerstate", parg1, "bad ordinal(%d) powerstate %d power state %d ", arg2, arg3, arg4 );
			}
			else
				log( info, "HubPolicyMaker", "setpowerstate", parg1, "whatDevice != this power state ordinal(%d) whatDevice 0x%x power state %d ", arg2, arg3, arg4 );
			break;

		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}


static void 
CollectTraceCompositeDriver ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_COMPOSITE_DRIVER_TRACE( kTPCompositeDriverConfigureDevice ):
			if ( arg4 == 1 )
			{
				log( info, "Composite", "Config Device", parg1, "GetFullConfigDescriptor(%d) returned NULL, retrying ", arg2 );
			}
			else if ( arg4 == 2 )
				log( info, "Composite", "Config Device", parg1, "returned NULL, retrying ");
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}
#pragma mark Actions Tracepoints

static void 
CollectTraceOutstandingIO ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_OUTSTANDING_IO_TRACE( kTPHIDDecrement ):
			log( info, "OutstandingIO", "HIDDecrement", parg1, "%d ", arg2 );
			break;
		
		case USB_OUTSTANDING_IO_TRACE( kTPHIDIncrement ):
			log( info, "OutstandingIO", "HIDIncrement", parg1, "%d ", arg2 );
			break;
			
		case USB_OUTSTANDING_IO_TRACE( kTPHubDecrement ):
			log( info, "OutstandingIO", "HubDecrement", parg1, "serial #: %d, IO: %d, needInterruptRead: %s", arg2, arg3, arg4 ? "yes" : "no" );
			break;

		case USB_OUTSTANDING_IO_TRACE( kTPHubIncrement ):
			log( info, "OutstandingIO", "HubIncrement", parg1, "IO: %d", arg2 );
			break;

		case USB_OUTSTANDING_IO_TRACE( kTPInterfaceUCDecrement ):
			log( info, "OutstandingIO", "InterfaceUCDecrement", parg1, "%d ", arg2 );
			break;

		case USB_OUTSTANDING_IO_TRACE( kTPInterfaceUCIncrement ):
			log( info, "OutstandingIO", "InterfaceUCIncrement", parg1, "%d ", arg2 );
			break;

		case USB_OUTSTANDING_IO_TRACE( kTPDeviceUCDecrement ):
			log( info, "OutstandingIO", "DeviceUCDecrement", parg1, "%d ", arg2 );
			break;

		case USB_OUTSTANDING_IO_TRACE( kTPDeviceUCIncrement ):
			log( info, "OutstandingIO", "DeviceUCIncrement", parg1, "%d ", arg2 );
			break;
		
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}
	

#pragma mark Other Driver Tracepoints
static void 
CollectTraceAudioDriver ( kd_buf tracepoint ) 
{
	uint32_t 			debugID;
	uint32_t 			type;
	int					qualifier;
	uintptr_t			parg1, parg2, parg3, parg4;
	uint32_t			arg1, arg2, arg3, arg4;
	time_t 				currentTime;
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	arg1 = (uint32_t)parg1;
	arg2 = (uint32_t)parg2;
	arg3 = (uint32_t)parg3;
	arg4 = (uint32_t)parg4;
	
	trace_info info;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.debugid = tracepoint.debugid;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	currentTime = time ( NULL );
	
	switch ( type )
	{
		case USB_AUDIO_DRIVER_TRACE( kTPAudioDriverRead ):
			log( info, "AUAudio", "PrepareFrameList", parg1, "frameListPtr: 0x%x, frame start: %d, first frame: %d", arg2, arg3, arg4);
			break;
		case USB_AUDIO_DRIVER_TRACE( kTPAudioDriverCoalesceInputSamples ):
			log( info, "AUAudio", "CoalesceInputSamples", 0, "frameListPtr: 0x%x, numBytesToCoalesce: %d, mCurrentFrameList: 0x%x, pFrames[0].frStatus: 0x%x", arg1, arg2, arg3, arg4);
			break;
		case USB_AUDIO_DRIVER_TRACE( kTPAudioDriverCoalesceError ):
			log( info, "AUAudio", "CoalesceInputSamples", parg1, "Error:  frameListPtr: 0x%x, frActCount: %d, frStatus:0x%x", arg2, arg3, arg4);
			break;
			
		case USB_AUDIO_DRIVER_TRACE( kTPAudioDriverCoalesce ):
			static	uint32_t auaTime = 0;
#ifdef __LP64__
			log( info, "AUAudio", "CoalesceInputSamples", 0, "Read:  frameListPtr: 0x%x, frActCount: %d, frStatus:0x%x, frTimeStamp: 0x%qx", arg1, arg2, arg3, (uint64_t)arg4);
#else
			log( info, "AUAudio", "CoalesceInputSamples", 0, "Read:  frameListPtr: 0x%x, frActCount: %d, frStatus:0x%x, frTimeStamp.lo: 0x%x ( %qd ) delta: %d", arg1, arg2, arg3, arg4, (uint64_t)arg4, arg4-auaTime);
#endif
			auaTime = arg4;
			
			break;
			
		case USB_AUDIO_DRIVER_TRACE( kTPAudioDriverReadHandler ):
			log( info, "AUAudio", "ReadHandler", 0, "frameListPtr: 0x%x, currentUSBFrame: %d, mCurrentFrameList: 0x%x, result: 0x%x", arg1, arg2, arg3, arg4);
			break;
			
		case USB_AUDIO_DRIVER_TRACE( kTPAudioDriverCoalesceError2 ):
			log( info, "AUAudio", "CoalesceInputSamples", parg1, "Error:  Requested %d, Remaining: %d on framelist 0x%x", arg2, arg3, arg4);
			break;
			
		case USB_AUDIO_DRIVER_TRACE( kTPAudioDriverConvertInputSamples ):
			log( info, "AUAudio", "convertInputSamples", parg1, "firstSampleFrame: %d, numSampleFrames: %d", arg2, arg3);
			break;
			
		default:
			CollectTraceUnknown( tracepoint );
			break;	
	} // End of switch
}

#pragma mark Raw File Support

//———————————————————————————————————————————————————————————————————————————
//	ReadRawFile
//———————————————————————————————————————————————————————————————————————————

static void
ReadRawFile( const char * filepath )
{
	FILE * file = fopen( filepath, "r");
	
	raw_data_t kp;
	bzero( &kp, sizeof(raw_data_t) );
	
	if ( file )
	{
		while ( fread( &kp, sizeof(raw_data_t), 1, file ) )
		{
			kd_buf tracepoint;
			bzero( &tracepoint, sizeof(kd_buf) );
			
			if ( kp.debugid == kInvalid )
			{
				vlog( "Found an invalid entry in raw file.\n");
				continue;
			}
			
			if ( kp.debugid == kDivisorEntry )
			{
				gDivisor = (double)(kp.timestamp);
				vlog("Found divisor %f as 0x%llx\n", gDivisor, kp.timestamp);
			}
			else
			{
				tracepoint.timestamp = kp.timestamp;	// don't mask before writing
				tracepoint.arg1 = kp.arg1;
				tracepoint.arg2 = kp.arg2;
				tracepoint.arg3 = kp.arg3;
				tracepoint.arg4 = kp.arg4;
				tracepoint.arg5 = kp.arg5;
				tracepoint.debugid = kp.debugid;
				kdbg_set_cpu(&tracepoint, kp.cpuid);
				
				//printf("0x%llx 0x%08x %8lx  %8lx  %8lx  %8lx\n", tracepoint.timestamp, tracepoint.debugid, tracepoint.parg1, tracepoint.arg2, tracepoint.arg3, tracepoint.arg4 );
				
				// send tracepoint to be processed
				ProcessTracepoint( tracepoint );
			}
		}
		
		fclose( file );
	}
	else
		Quit( "Could not open raw file to read!\n");
}

//———————————————————————————————————————————————————————————————————————————
//	CollectToRawFile
//———————————————————————————————————————————————————————————————————————————

static void
CollectToRawFile ( FILE * file )
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
		Quit ( "trace facility failure, KERN_KDREADTR\n");
	
	count = (int)needed;
	
	if ( bufinfo.flags & KDBG_WRAPPED )
	{
		EnableTraceBuffer ( 0 );
		EnableTraceBuffer ( 1 );
	}
		
	raw_data_t rawkd;
	
	for ( index = 0; index < count; index++ )
	{
		if ( gTraceBuffer[index].debugid == kInvalid )
		{
			vlog( "Tracepoint %d is invalid.\n", index);
			continue;
		}
		
		bzero(&rawkd, sizeof(raw_data_t));
		rawkd.timestamp = gTraceBuffer[index].timestamp;	// don't mask before writing
		rawkd.arg1 = gTraceBuffer[index].arg1;
		rawkd.arg2 = gTraceBuffer[index].arg2;
		rawkd.arg3 = gTraceBuffer[index].arg3;
		rawkd.arg4 = gTraceBuffer[index].arg4;
		rawkd.arg5 = gTraceBuffer[index].arg5;
		rawkd.debugid = gTraceBuffer[index].debugid;
		rawkd.delta = 0; // set != 0 to report function-specific delta to be printed
		rawkd.cpuid = kdbg_get_cpu(&gTraceBuffer[index]);
		
		errno = 0;
		fwrite( (const void *)&rawkd, sizeof(raw_data_t), 1, file );
		if ( errno )
			elog("Error %d occurred writing data with debugid 0x%x and timestamp 0x%llx\n", errno, rawkd.debugid, kdbg_get_timestamp(&gTraceBuffer[index]));
		
		// send tracepoint to be processed as well
		ProcessTracepoint( gTraceBuffer[index] );
	}
}

//———————————————————————————————————————————————————————————————————————————
//	PrependDivisorEntry
//———————————————————————————————————————————————————————————————————————————

static void
PrependDivisorEntry ( FILE * file )
{
	raw_data_t rawkd;
	bzero(&rawkd, sizeof(raw_data_t));
	
	rawkd.timestamp = (uint64_t)gDivisor;
	rawkd.debugid = kDivisorEntry;
	vlog("Inserting divisor %f as 0x%llx\n", gDivisor, rawkd.timestamp);
	
	errno = 0;
	fwrite( (const void *)&rawkd, sizeof(raw_data_t), 1, file );
	if ( errno )
		elog("Error %d writing divisor data\n", errno);
}

#pragma mark Convenience

static void
CollectTraceUnknown ( kd_buf tracepoint )
{
	uint32_t 			debugID;
	uint32_t 			type;
	uint32_t			qualifier;
	uint32_t			tpClass, tpGroup, tpCode;
	uintptr_t			parg1, parg2, parg3, parg4;
	//uint32_t			parg1, arg2, arg3, arg4;
	uintptr_t			thread;
	int					cpu; 
	char				timestring[kTimeStringSize];
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	qualifier = debugID & 0x3;
	
	tpClass = (debugID & 0xFFFF0000) >> 16;
	tpGroup = (debugID & 0x0000FC00) >> 10;
	tpCode = (debugID & 0x000003FC) >> 2;
	
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	
	thread = tracepoint.arg5;
	cpu = kdbg_get_cpu(&tracepoint);
	
	trace_info info;
	info.debugid = tracepoint.debugid;
	info.timestamp = kdbg_get_timestamp(&tracepoint);
	info.thread = tracepoint.arg5;
	info.cpuid = kdbg_get_cpu(&tracepoint);
	
	char qStrArray[4];
	const char * qualString = (char *)qStrArray;
	if ( qualifier == 1 )
		qualString = kPrintEndToken;
	else if ( qualifier == 2 )
		qualString = kPrintStartToken;
	else
		qualString = kPrintMedialToken;
	
	if ( (debugID & 0xFFFF0000) == kTPAllUSB )
	{
		printf(
#ifdef __LP64__
			   "%s %s F-0x%04x|%02u|%03u  %16lx  %16lx  %16lx  %16lx  0x%16lx  %2u\n",
#else
			   "%s %s F-0x%04x|%02u|%03u  %8lx  %8lx  %8lx  %8lx  0x%8lx  %2u\n",
#endif
			   ConvertTimestamp( kdbg_get_timestamp(&tracepoint), timestring ), qualString , tpClass, tpGroup, tpCode, parg1, parg2, parg3, parg4, thread, cpu );
	}
	else if ( gPrintMask & kPrintMaskAllTracepoints )
	{
		if ( gCodesFilePath[0] != 0 )
		{
			char description[64];
			
			if ( !DecodeID(type, (char *)description, 64 ) )
			{
				char digits[16];
				snprintf(digits, 16, "U-0x%04x|%02u|%03u", tpClass, tpGroup, tpCode);	// return id in string form
				snprintf(description, 64, "%s", digits);	// pad string
			}
			
			log(info, "", description, NULL,
#ifdef __LP64__
				"%16lx  %16lx  %16lx  %16lx  0x%16lx  %2u",
#else
				"%8lx  %8lx  %8lx  %8lx  0x%8lx  %2u",
#endif
				parg1, parg2, parg3, parg4, thread, cpu );
		}
		else
		{
			printf(
#ifdef __LP64__
				   "%s %s U-0x%04x|%02u|%03u  %16lx  %16lx  %16lx  %16lx  0x%16lx  %2u\n",
#else
				   "%s %s U-0x%04x|%02u|%03u  %8lx  %8lx  %8lx  %8lx  0x%8lx  %2u\n",
#endif
				   ConvertTimestamp( kdbg_get_timestamp(&tracepoint), timestring ), qualString , tpClass, tpGroup, tpCode, parg1, parg2, parg3, parg4, thread, cpu );
		}
	}
}

//———————————————————————————————————————————————————————————————————————————
//	DecodeID
//———————————————————————————————————————————————————————————————————————————

static char *
DecodeID ( uint32_t id, char * string, const int max )
{
	// need to lookup code in codefile
	
	if ( gCodesFileStream == NULL )
	{
		elog ("Error opening file '%s'\n", gCodesFilePath);
	}
	else
	{
		fseek( gCodesFileStream, 0, SEEK_SET );
		
		bzero( string, max );
		
		char buf[64];
		char * line = buf;
		bzero( line, 64 );
		char * desc = NULL;
		
		bool match = false;
		
		// zzz shouldn't read the whole code file on each trace ... should probably store in memory
		while ( desc = fgets( line, max, gCodesFileStream ) )
		{
			char * codestring = strsep( &desc, "\t ");
			unsigned long code = strtoul(codestring, NULL, 0);
			
			if ( code == id )
			{
				match = true;
				break;
			}
		}
		
		if ( match )
		{
			char * final = desc;
			
			int i;
			int length = (int)strlen(desc);	// desc null terminated by fgets
			
			// prettify code string
			for ( i = 0; i < length; i++ )
			{
				if ( desc[i] == ' ' || desc[i] == '\t' )
				{
					final++;
				}
				else if ( desc[i] == '\n' )
				{
					desc[i] = '\0';
					break;
				}
			}
			
			// overwrite whitespace characters at the beginning of char array
			// copy into string (rather than in place) so the pointer doesn't change
			snprintf(string, max, "%s", final);
		}
		else
		{
			//char digits[11];
			//snprintf(digits, 11, "%#010x", id);	// return id in string form
			//snprintf(string, max, "%-30s", digits);	// pad string
			return NULL;	// leave digit to string conversion to call so they can pad as necessary
		}
	}
	
	return string;
}

//———————————————————————————————————————————————————————————————————————————
//	CollectTraceBasic
//———————————————————————————————————————————————————————————————————————————

static void
CollectTraceBasic ( kd_buf tracepoint )
{
	uint32_t 			debugID, type;
	uintptr_t			parg1, parg2, parg3, parg4, thread;
	int					cpu;
	char				description[64];
	
	debugID = tracepoint.debugid;
	type = debugID & ~(DBG_FUNC_START | DBG_FUNC_END);
	parg1 = tracepoint.arg1;
	parg2 = tracepoint.arg2;
	parg3 = tracepoint.arg3;
	parg4 = tracepoint.arg4;
	thread = tracepoint.arg5;
	cpu = kdbg_get_cpu(&tracepoint);
	
	uint64_t timestamp = kdbg_get_timestamp(&tracepoint);
	
	if ( !gStartingAbsTime )
	{
		gStartingAbsTime = timestamp;
		gLastTimeStamp = timestamp;
	}
		
	double elapsed = timestamp - gStartingAbsTime;
	elapsed /= 1000;
	double delta = timestamp - gLastTimeStamp;
	delta /= 1000;
	gLastTimeStamp = timestamp;
	
	if ( gCodesFilePath[0] != 0 )
	{
		if ( !DecodeID(type, (char *)description, 64 ) )
		{
			char digits[11];
			snprintf(digits, 11, "%#010x", debugID);	// return id in string form
			snprintf(description, 64, "%s", digits);	// pad string
		}
		
		printf(
#ifdef __LP64__
			   "%10.1f %5.1f  %-30s %16lx  %16lx  %16lx  %16lx  0x%016lx  %2u\n",
#else
			   "%10.1f %5.1f  %-30s %8lx  %8lx  %8lx  %8lx  0x%08lx  %2u\n",
#endif
			   elapsed, delta, description, parg1, parg2, parg3, parg4, thread, cpu );
		
	}
	else
	{
		printf(
#ifdef __LP64__
			   "%10.1f %5.1f  0x%08x  %16lx  %16lx  %16lx  %16lx  0x%016lx  %2u\n",
#else
			   "%10.1f %5.1f  0x%08x  %8lx  %8lx  %8lx  %8lx  0x%08lx  %2u\n",
#endif
			   elapsed, delta, debugID, parg1, parg2, parg3, parg4, thread, cpu );
	}
}	

//———————————————————————————————————————————————————————————————————————————
//	ConvertTimestamp
//  - takes actual timestamp, masked and such if necessary
//———————————————————————————————————————————————————————————————————————————

static char *
ConvertTimestamp ( uint64_t timestamp, char * timestring )
{
	uint64_t			secs, milli, micro;
	uint64_t			mins;
	time_t 				currentTime;
	
	int index = 0;
	
	if ( gTimeStampMask & kTimeStampLocalTime )
	{
		currentTime = time ( NULL );
		
		snprintf(timestring+index, 7, "%-5.5s-", &( ctime ( &currentTime )[11] ) );
		index += 6;
	}
	
	if ( gTimeStampMask & kTimeStampKernel )
	{
		micro = timestamp / gDivisor;
		milli = micro / 1000;
		secs = (milli / 1000);
		mins = secs / 60;
		
		//snprintf(timestring+index, 11, "%02llu:%03llu:%03llu", secs-(mins*60), milli-(secs*1000), micro-(milli*1000) );
		snprintf(timestring+index, 11, "%02llu%03llu.%03llu", secs-(mins*60), milli-(secs*1000), micro-(milli*1000) );
		index += 10;
	}
	
	return timestring;
}

//———————————————————————————————————————————————————————————————————————————
//	PrintHeader - called by log()
//  - info contains actual timestamp and cpu, masked and such if necessary
//———————————————————————————————————————————————————————————————————————————

static bool
PrintHeader ( trace_info info, const char * group, const char * method, uintptr_t fwim )
{
	uint32_t debugID = info.debugid;
	
	uint64_t timestamp = info.timestamp;
	char timestring[kTimeStringSize];
	
	if ( gTimeStampMask & kTimeStampKernel )
		printf ( "%s ", ConvertTimestamp( timestamp, timestring ) );
	
	if ( gPrintCPU )
	{
		uint32_t cpu = info.cpuid;
		printf( "%-2u ", cpu);
	}
	
	if ( gPrintThread )
	{
		uintptr_t thread = info.thread;
		printf(
#ifdef __LP64__
			   "0x%016lx ",
#else
			   "0x%08lx ",
#endif
			   thread);
	}
	
	if ( gPrintCodes )
		printf ("0x%08x ", debugID);
	
	uint32_t qualifier = debugID & 0x3;
	
	if ( qualifier == DBG_FUNC_START )
	{
		printf( "%s ", kPrintStartToken );
		if ( gPrintIndent )
			IndentIn( gNumIndentTabs );
	}
	else if ( qualifier == DBG_FUNC_END )
	{
		printf( "%s ", kPrintEndToken );
		if ( gPrintIndent )
			IndentOut( gNumIndentTabs );
	}
	else
	{
		printf( "%s ", kPrintMedialToken );
		if ( gPrintIndent )
			Indent( gNumIndentTabs );
	}
	
	char description[74];
	snprintf( description, 74, "%s%s%s", gPrintGroup ? group : "", gPrintGroup ? "::": "", gPrintMethod ? method : "");
	
	if ( gPrintNoSep )
	{
		printf( "%s ", description );
	}
	else
	{
		if ( !gPrintMethod )
			printf( "%-10s ", description );
		else
			printf( "%-30s ", description );
	}
	
	if ( gPrintUSBP )
	{
#ifdef __LP64__
		printf("(0x%16.016qx) ", (uint64_t)fwim);
#else
		printf("(0x%8.08x) ", (uint32_t)fwim);
#endif
	}
	
	return true;
}

//———————————————————————————————————————————————————————————————————————————
//	Indent
//———————————————————————————————————————————————————————————————————————————

static void
Indent ( int numOfTabs )
{
	int i;
	
	for ( i = 0; i < gNumIndentTabs; i++ )
	{
		printf( " ");
	}
}

//———————————————————————————————————————————————————————————————————————————
//	IndentIn
//———————————————————————————————————————————————————————————————————————————

static void
IndentIn ( int numOfTabs )
{
	Indent( numOfTabs );
	gNumIndentTabs++;
}

//———————————————————————————————————————————————————————————————————————————
//	IndentOut
//———————————————————————————————————————————————————————————————————————————

static void
IndentOut ( int numOfTabs )
{
	Indent( numOfTabs );
	gNumIndentTabs--;
}

//———————————————————————————————————————————————————————————————————————————
//	Quit
//———————————————————————————————————————————————————————————————————————————

static void
Quit ( const char * s )
{	
	if ( gCodesFileStream )
		fclose( gCodesFileStream );
	
	if ( gLogFileStream )
		fclose( gLogFileStream );
	
	if ( gTraceEnabled == TRUE )
		EnableTraceBuffer ( 0 );
	
	if ( gSetRemoveFlag == TRUE )
		RemoveTraceBuffer ( );
	
	ResetDebugFlags ( );
	
	if ( s != NULL )
	{
		elog( "%s: %s\n", gProgramName, s );
		exit ( 1 );
	}
	else
	{
		exit ( 0 );
	}	
}


//-----------------------------------------------------------------------------
//	ResetDebugFlags
//-----------------------------------------------------------------------------

static void
ResetDebugFlags ( void )
{
	
	USBSysctlArgs usbArgs;
	int error;	
	
	usbArgs.type 		= kUSBTypeDebug;
	usbArgs.operation	= kUSBOperationSetFlags;
	usbArgs.debugFlags 	= gSavedUSBDebugMask;
	
	error = sysctlbyname ( USB_SYSCTL, NULL, NULL, &usbArgs, sizeof ( usbArgs ) );
	if ( error != 0 )
	{
		elog( "sysctlbyname failed to set new 'usb' debug flags\n");
	}
}


//———————————————————————————————————————————————————————————————————————————
//	GetDivisor
//———————————————————————————————————————————————————————————————————————————

static void
GetDivisor ( void )
{
	
	struct mach_timebase_info	mti;
	
	mach_timebase_info ( &mti );
	
	gDivisor = ( ( double ) mti.denom / ( double ) mti.numer) * 1000;
	
}

const char * DecodeUSBTransferType( uint32_t type )
{
	switch (type)
	{
		case kUSBControl: return "Control";
		case kUSBIsoc: return "Isoc";
		case kUSBBulk: return "Bulk";
		case kUSBInterrupt: return "Interrupt";
		case kUSBAnyType: 
		default: return "Any";
	}
	
}
