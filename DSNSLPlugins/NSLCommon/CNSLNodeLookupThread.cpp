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
 *  @header CNSLNodeLookupThread
 */

#include "CNSLHeaders.h"

CNSLNodeLookupThread::CNSLNodeLookupThread( CNSLPlugin* parentPlugin )
{
	DBGLOG( "CNSLNodeLookupThread::CNSLNodeLookupThread\n" );
    mParentPlugin = parentPlugin;
}

CNSLNodeLookupThread::~CNSLNodeLookupThread()
{
	DBGLOG( "CNSLNodeLookupThread::~CNSLNodeLookupThread\n" );
    mParentPlugin->NodeLookupComplete();
}

void CNSLNodeLookupThread::AddResult( CFStringRef newNodeName )
{
	DBGLOG( "CNSLNodeLookupThread::AddResult (CFStringRef)\n" );
	
	if ( CFStringFind(newNodeName, CFSTR("/"), 0).length > 0 )
	{
		CFMutableStringRef theString = CFStringCreateMutableCopy( NULL, 0, newNodeName );

		CFStringFindAndReplace(theString, CFSTR("/"), CFSTR("\\047"), CFRangeMake(0,CFStringGetLength(newNodeName)), 0);
		mParentPlugin->AddNode( theString );
		CFRelease( theString );
	}
	else
		mParentPlugin->AddNode( newNodeName );
}
