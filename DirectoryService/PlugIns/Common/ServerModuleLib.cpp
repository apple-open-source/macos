/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/*!
 * @header ServerModuleLib
 */

#include <stdio.h>		// for vfprintf()
#include <string.h>		// for memcpy(), strcpy() and strlen()

#include "ServerModuleLib.h"

using namespace DSServerPlugin;

// ----------------------------------------------------------------------------
//	* Private Globals
// ----------------------------------------------------------------------------
static SvrLibFtbl	_Funcs = { nil, nil, nil, 0 };


// ----------------------------------------------------------------------------
//	* Skeletal implementations of server library functions.
//
// ----------------------------------------------------------------------------

void SetupLinkTable ( SvrLibFtbl *inTable )
{
	if ( _Funcs.registerNode )
	{
		return;
	}

	_Funcs = *inTable;

} // SetupLinkTable


//--------------------------------------------------------------------------------------------------
//	* DSRegisterNode()
//
//--------------------------------------------------------------------------------------------------

sInt32 DSRegisterNode ( const uInt32 inToken, tDataList *inNode, eDirNodeType inNodeType )
{
	if ( !_Funcs.registerNode )
	{
		return( -2802 );
	}

	return( (*_Funcs.registerNode)(inToken, inNode, inNodeType) );

} // DSRegisterNode


//--------------------------------------------------------------------------------------------------
//	* DSUnregisterNode()
//
//--------------------------------------------------------------------------------------------------

sInt32 DSUnregisterNode ( const uInt32 inToken, tDataList *inNode )
{
	if ( !_Funcs.unregisterNode )
	{
		return( -2802 );
	}

	return( (*_Funcs.unregisterNode)(inToken, inNode) );

} // DSUnregisterNode


//--------------------------------------------------------------------------------------------------
//	* DSDebugLog()
//
//--------------------------------------------------------------------------------------------------

sInt32 DSDebugLog ( const char *inFormat, va_list inArgs )
{
	if ( !_Funcs.debugLog )
	{
		return( -2802 );
	}

	(*_Funcs.debugLog)( inFormat, inArgs );

	return( noErr );

} // DSDebugLog
