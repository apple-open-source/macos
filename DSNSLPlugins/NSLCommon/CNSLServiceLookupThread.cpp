/*
 *  CNSLServiceLookupThread.cpp
 *
 *	This is a wrapper class for service lookups of plugins of NSL
 *
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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
//    mParentPlugin->LockPlugin();

	if ( mNeedToNotifyNodeToSearchWhenComplete )
	{
		// call the node to say we are going away.  The node will now wait until all its searches finish
		mNodeToSearch->ServiceLookupComplete( this );
	}
	
    if ( !AreWeCanceled() )
    {
//        mParentPlugin->ServiceLookupComplete( this, mNodeToSearch );		// only call this if we haven't been canceled
        mCanceled = true;
    }
    
    if ( mServiceType )
        ::CFRelease( mServiceType );

    if ( mNodeName )
        CFRelease( mNodeName );
//    mParentPlugin->UnlockPlugin();
}

void CNSLServiceLookupThread::Resume( void )
{
//    mParentPlugin->LockPlugin();

    if ( !AreWeCanceled() )
	{
        mNodeToSearch->StartingNewLookup( this );
		mNeedToNotifyNodeToSearchWhenComplete = true;
		
//    mParentPlugin->UnlockPlugin();
    
		if ( !AreWeCanceled() )
			DSLThread::Resume();
	}
}

void CNSLServiceLookupThread::AddResult( CNSLResult* newResult )
{
	DBGLOG( "CNSLServiceLookupThread::AddResult\n" );
//    mParentPlugin->LockPlugin();
	if ( !newResult->GetAttributeRef( CFSTR(kDSNAttrLocation) ) )
		newResult->AddAttribute( CFSTR(kDSNAttrLocation), mNodeToSearch->GetNodeName() );
    
    if ( !AreWeCanceled() )
        mNodeToSearch->AddService( newResult );
    
//    mParentPlugin->UnlockPlugin();
}








