/*
  File: DebugSupport.h

  Contains: 

  Version: 1.0.0

  Copyright: Copyright (c) 2007 by Apple Inc., All Rights Reserved.

Disclaimer:IMPORTANT:  This Apple software is supplied to you by Apple Inc. 
("Apple") in consideration of your agreement to the following terms, and your use, 
installation, modification or redistribution of this Apple software constitutes acceptance 
of these terms.  If you do not agree with these terms, please do not use, install, modify or 
redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under Apple's
copyrights in this original Apple software (the "Apple Software"), to use, reproduce, 
modify and redistribute the Apple Software, with or without modifications, in source and/or
binary forms; provided that if you redistribute the Apple Software in its entirety
and without modifications, you must retain this notice and the following text
and disclaimers in all such redistributions of the Apple Software.  Neither the
name, trademarks, service marks or logos of Apple Inc. may be used to
endorse or promote products derived from the Apple Software without specific prior
written permission from Apple.  Except as expressly stated in this notice, no
other rights or licenses, express or implied, are granted by Apple herein,
including but not limited to any patent rights that may be infringed by your derivative
works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE
OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. IN NO EVENT SHALL APPLE 
BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef __DEBUG_SUPPORT_H__
#define __DEBUG_SUPPORT_H__


#if KERNEL
#include <IOKit/IOLib.h>
#else
#include <IOKit/IOKitLib.h>
#endif


#ifndef DEBUG_ASSERT_COMPONENT_NAME_STRING
	#define DEBUG_ASSERT_COMPONENT_NAME_STRING "AppleSCSIEmulator"
#endif

#if KERNEL

#ifdef __cplusplus
extern "C" {
#endif

/* AppleSCSIEmulatorDebugAssert prototype*/
void
AppleSCSIEmulatorDebugAssert (
						const char * componentNameString,
						const char * assertionString, 
						const char * exceptionLabelString,
						const char * errorString,
						const char * fileName,
						long lineNumber,
						int errorCode );

#ifdef __cplusplus
}
#endif


#define DEBUG_ASSERT_MESSAGE( componentNameString, 					\
	assertionString, 												\
	exceptionLabelString, 											\
	errorString, 													\
	fileName, 														\
	lineNumber, 													\
	error ) 														\
	AppleSCSIEmulatorDebugAssert( componentNameString,				\
	assertionString, 												\
	exceptionLabelString, 											\
	errorString, 													\
	fileName, 														\
	lineNumber, 													\
	error )


#endif	/* KERNEL */


#include </usr/include/AssertMacros.h>


#define require_success( errorCode, exceptionLabel ) \
	require( kIOReturnSuccess == (errorCode), exceptionLabel )

#define require_success_action( errorCode, exceptionLabel, action ) \
	require_action( kIOReturnSuccess == (errorCode), exceptionLabel, action )

#define require_success_quiet( errorCode, exceptionLabel ) \
	require_quiet( kIOReturnSuccess == (errorCode), exceptionLabel )

#define require_success_action_quiet( errorCode, exceptionLabel, action ) \
	require_action_quiet( kIOReturnSuccess == (errorCode), exceptionLabel, action )

#define require_success_string( errorCode, exceptionLabel, message ) \
	require_string( kIOReturnSuccess == (errorCode), exceptionLabel, message )

#define require_success_action_string( errorCode, exceptionLabel, action, message ) \
	require_action_string( kIOReturnSuccess == (errorCode), exceptionLabel, action, message )


#define require_nonzero( obj, exceptionLabel ) \
	require( ( 0 != obj ), exceptionLabel )

#define require_nonzero_action( obj, exceptionLabel, action ) \
	require_action( ( 0 != obj ), exceptionLabel, action )

#define require_nonzero_quiet( obj, exceptionLabel ) \
	require_quiet( ( 0 != obj ), exceptionLabel )

#define require_nonzero_action_quiet( obj, exceptionLabel, action ) \
	require_action_quiet( ( 0 != obj ), exceptionLabel, action )

#define require_nonzero_string( obj, exceptionLabel, message ) \
	require_string( ( 0 != obj ), exceptionLabel, message )

#define require_nonzero_action_string( obj, exceptionLabel, action, message ) \
	require_action_string( ( 0 != obj ), exceptionLabel, action, message )


#endif	/* __DEBUG_SUPPORT_H__ */
