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
 *  @header CNSLServiceLookupThread
 */

#include "CNSLHeaders.h"

CNSLServiceLookupThread::CNSLServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "CNSLServiceLookupThread::CNSLServiceLookupThread\n" );
    mParentPlugin = parentPlugin;
    mNodeToSearch = nodeDirRep;
	mNeedToNotifyNodeToSearchWhenComplete = false;		// only set this if we told it we were starting
    mCanceled = false;
    mNodeName = mNodeToSearch->GetNodeName();
    CFRetain( mNodeName );
	
	mNodeToSearch->Retain();
    
    if ( serviceType )
        mServiceType = ::CFStringCreateWithCString( NULL, serviceType, kCFStringEncodingUTF8 );
    else
    {
        DBGLOG( "CNSLServiceLookupThread::CNSLServiceLookupThread someone passed in a null serviceType!" );
        mServiceType = ::CFStringCreateWithCString( NULL, "NULL_ServiceType", kCFStringEncodingUTF8 );
    }
}

CNSLServiceLookupThread::~CNSLServiceLookupThread()
{
	DBGLOG( "CNSLServiceLookupThread::~CNSLServiceLookupThread\n" );

	if ( mNeedToNotifyNodeToSearchWhenComplete )
	{
		// call the node to say we are going away.  The node will now wait until all its searches finish
		mNodeToSearch->ServiceLookupComplete( this );
	}
	
	mNodeToSearch->Release();

    if ( !AreWeCanceled() )
    {
        mCanceled = true;
    }
    
    if ( mServiceType )
        ::CFRelease( mServiceType );

    if ( mNodeName )
        CFRelease( mNodeName );
}

void CNSLServiceLookupThread::Resume( void )
{
	try
	{
		if ( !AreWeCanceled() )
		{
			mNodeToSearch->StartingNewLookup( this );
			mNeedToNotifyNodeToSearchWhenComplete = true;
			
			if ( !AreWeCanceled() )
				DSLThread::Resume();
		}
	}

	catch (...)
	{
	}
}

void CNSLServiceLookupThread::AddResult( CNSLResult* newResult )
{
	DBGLOG( "CNSLServiceLookupThread::AddResult\n" );

	if ( !newResult->GetAttributeRef( kDSNAttrLocationSAFE_CFSTR ) )
		newResult->AddAttribute( kDSNAttrLocationSAFE_CFSTR, mNodeToSearch->GetNodeName() );
    
    if ( !AreWeCanceled() )
        mNodeToSearch->AddService( newResult );
}
