/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
/*!
 *  @header NISRecordLookupThread
 */

#include "NISHeaders.h"

NISRecordLookupThread::NISRecordLookupThread( CNSLPlugin* parentPlugin, char* serviceType, NISDirNodeRep* nodeDirRep )
{
	DBGLOG( "NISRecordLookupThread::NISRecordLookupThread\n" );
    mParentPlugin = parentPlugin;
    mNodeToSearch = nodeDirRep;
	mNeedToNotifyNodeToSearchWhenComplete = false;		// only set this if we told it we were starting
    mCanceled = false;
    mNodeName = mNodeToSearch->GetNodeName();
    CFRetain( mNodeName );
    
    if ( serviceType )
        mServiceType = ::CFStringCreateWithCString( NULL, serviceType, kCFStringEncodingUTF8 );
    else
    {
        DBGLOG( "NISRecordLookupThread::NISRecordLookupThread someone passed in a null serviceType!" );
        mServiceType = ::CFStringCreateWithCString( NULL, "NULL_ServiceType", kCFStringEncodingUTF8 );
    }
}

NISRecordLookupThread::~NISRecordLookupThread()
{
	DBGLOG( "NISRecordLookupThread::~NISRecordLookupThread\n" );

	if ( mNeedToNotifyNodeToSearchWhenComplete )
	{
		// call the node to say we are going away.  The node will now wait until all its searches finish
		mNodeToSearch->ServiceLookupComplete( this );
	}
	
    if ( !AreWeCanceled() )
    {
        mCanceled = true;
    }
    
    if ( mServiceType )
        ::CFRelease( mServiceType );

    if ( mNodeName )
        CFRelease( mNodeName );
}

void NISRecordLookupThread::Resume( void )
{
    if ( !AreWeCanceled() )
	{
        mNodeToSearch->StartingNewLookup( this );
		mNeedToNotifyNodeToSearchWhenComplete = true;
		
		if ( !AreWeCanceled() )
			DSLThread::Resume();
	}
}

void NISRecordLookupThread::AddResult( NISResult* newResult )
{
	DBGLOG( "NISRecordLookupThread::AddResult\n" );

	if ( !newResult->GetAttributeRef( CFSTR(kDSNAttrLocation) ) )
		newResult->AddAttribute( CFSTR(kDSNAttrLocation), mNodeToSearch->GetNodeName() );
    
    if ( !AreWeCanceled() )
        mNodeToSearch->AddService( newResult );
}
