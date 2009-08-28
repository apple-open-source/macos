/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
 *
 * IMPORTANT:  This Apple software is supplied to you by Apple Inc. ("Apple") in 
 * consideration of your agreement to the following terms, and your use, installation, 
 * modification or redistribution of this Apple software constitutes acceptance of these
 * terms.  If you do not agree with these terms, please do not use, install, modify or 
 * redistribute this Apple software.
 *
 * In consideration of your agreement to abide by the following terms, and subject to these 
 * terms, Apple grants you a personal, non exclusive license, under Apple’s copyrights in this 
 * original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute 
 * the Apple Software, with or without modifications, in source and/or binary forms; provided 
 * that if you redistribute the Apple Software in its entirety and without modifications, you 
 * must retain this notice and the following text and disclaimers in all such redistributions 
 * of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple 
 * Computer, Inc. may be used to endorse or promote products derived from the Apple Software 
 * without specific prior written permission from Apple. Except as expressly stated in this 
 * notice, no other rights or licenses, express or implied, are granted by Apple herein, 
 * including but not limited to any patent rights that may be infringed by your derivative 
 * works or by other works in which the Apple Software may be incorporated.
 * 
 * The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-
 * INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE 
 * SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 
 *
 * IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
 * REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
 * WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
 * OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CoreFoundation.h>
#include "Probing.h"


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 0

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "SCSITargetProberUnixTool"

#if DEBUG
#define DEBUG_ASSERT_MESSAGE(componentNameString,	\
							 assertionString,		\
							 exceptionLabelString,	\
							 errorString,			\
							 fileName,				\
							 lineNumber,			\
							 errorCode)				\
DebugAssert(componentNameString,					\
					   assertionString,				\
					   exceptionLabelString,		\
					   errorString,					\
					   fileName,					\
					   lineNumber,					\
					   errorCode)					\

static void
DebugAssert ( const char *	componentNameString,
			  const char *	assertionString,
			  const char *	exceptionLabelString,
			  const char *	errorString,
			  const char *	fileName,
			  long			lineNumber,
			  int			errorCode )
{
	
	if ( ( assertionString != NULL ) && ( *assertionString != '\0' ) )
		printf ( "Assertion failed: %s: %s\n", componentNameString, assertionString );
	else
		printf ( "Check failed: %s:\n", componentNameString );
	if ( exceptionLabelString != NULL )
		printf ( "	 %s\n", exceptionLabelString );
	if ( errorString != NULL )
		printf ( "	 %s\n", errorString );
	if ( fileName != NULL )
		printf ( "	 file: %s\n", fileName );
	if ( lineNumber != 0 )
		printf ( "	 line: %ld\n", lineNumber );
	if ( errorCode != 0 )
		printf ( "	 error: %d\n", errorCode );
	
}

#endif	/* DEBUG */

#include <AssertMacros.h>


//-----------------------------------------------------------------------------
//	Prototypes
//-----------------------------------------------------------------------------

static IOReturn
ParseArguments ( int argc, const char * argv[],
				 UInt64 * domainID, SCSITargetIdentifier * targetID );

static void
PrintUsage ( void );


//-----------------------------------------------------------------------------
//	main - Our main entry point
//-----------------------------------------------------------------------------

int
main ( int argc, const char * argv[] )
{
	
	int						returnCode	= 0;
	IOReturn				result		= kIOReturnSuccess;
	UInt64					domainID	= 0;
	SCSITargetIdentifier	targetID	= 0;
	
	result = ParseArguments ( argc, argv, &domainID, &targetID );
	require_action ( ( result == 0 ), ErrorExit, PrintUsage ( ); returnCode = 1 );
	
	printf ( "SCSITargetProber: Probing device for domain = %lld, targetID = %lld\n", domainID, targetID );
	
	result = ReprobeDomainTarget ( domainID, targetID );
	require_action ( ( result == 0 ), ErrorExit, printf ( "Error = %s (0x%08x) reprobing device\n", mach_error_string ( result ), result ); returnCode = 2 );
	
	return 0;
	
	
ErrorExit:
	
	
	return returnCode;
	
}


//-----------------------------------------------------------------------------
//	ParseArguments - Parses argument list
//-----------------------------------------------------------------------------

static IOReturn
ParseArguments ( int argc, const char * argv[],
				 UInt64 * domainID, SCSITargetIdentifier * targetID )
{
	
	IOReturn	result	= kIOReturnSuccess;
	int			ch;
	
	while ( ( ch = getopt ( argc, ( char * const * ) argv, "d:t:" ) ) != -1 )
	{
		
		switch ( ch )
		{
			
			case 'd':
				*domainID = strtoull ( optarg, ( char ** ) NULL, 10 );
				break;
			
			case 't':
				*targetID = strtoull ( optarg, ( char ** ) NULL, 10 );
				break;
			
			default:
				result = kIOReturnBadArgument;
				break;
			
		}
		
	}
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	PrintUsage - Prints out usage
//-----------------------------------------------------------------------------

void
PrintUsage ( void )
{
	
	printf ( "\n" );
	printf ( "Usage: stp -d domainID -t targetID\n" );
	printf ( "\t\t" );
	printf ( "-d This option specifies which SCSI Domain on which to find the target for probing\n" );
	printf ( "\t\t" );
	printf ( "-t This option specifices which SCSI Target Identifier should be probed\n" );
	printf ( "\n" );
	
}