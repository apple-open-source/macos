/*
 *  CNSLNodeLookupThread.cpp
 *
 *	This is a wrapper base class for getting node data to publish (via Neighborhood lookups in
 *	old NSL Plugins)
 *
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

//#include <Carbon/Carbon.h>

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
	DBGLOG( "CNSLNodeLookupThread::AddResult\n" );
    mParentPlugin->AddNode( newNodeName );
}

void CNSLNodeLookupThread::AddResult( const char* newNodeName )
{
	DBGLOG( "CNSLNodeLookupThread::AddResult\n" );
    mParentPlugin->AddNode( newNodeName );
}
