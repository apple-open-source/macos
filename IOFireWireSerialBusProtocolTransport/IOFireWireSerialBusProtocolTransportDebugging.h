/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IO_FIRE_WIRE_SERIAL_BUS_PROTOCOL_TRANSPORT_DEBUGGING_
#define _IO_FIRE_WIRE_SERIAL_BUS_PROTOCOL_TRANSPORT_DEBUGGING_

#include <IOKit/IOLib.h>

#if KERNEL

void IOFireWireSerialBusProtocolTransportDebugAssert ( const char * componentNameString,
                                                       const char * assertionString, 
                                                       const char * exceptionLabelString,
                                                       const char * errorString,
                                                       const char * fileName,
                                                       long lineNumber,
                                                       int errorCode );

#define DEBUG_ASSERT_MESSAGE( componentNameString, \
	assertionString, \
	exceptionLabelString, \
	errorString, \
	fileName, \
	lineNumber, \
	error ) \
	IOFireWireSerialBusProtocolTransportDebugAssert( componentNameString, \
	assertionString, \
	exceptionLabelString, \
	errorString, \
	fileName, \
	lineNumber, \
	error )


#endif	/* KERNEL */


#include </usr/include/AssertMacros.h>

#define DEUBUG_UNUSED( X )		( void )( X )

#endif	/* _IO_FIRE_WIRE_SERIAL_BUS_PROTOCOL_TRANSPORT_DEBUGGING_ */