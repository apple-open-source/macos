/*
 *  CNSLNodeLookupThread.h
 *
 *	This is a wrapper base class for getting node data to publish (via Neighborhood lookups in
 *	old NSL Plugins)
 *
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CNSLNodeLookupThread_
#define _CNSLNodeLookupThread_ 1

#include <Carbon/Carbon.h>
#include <DirectoryServiceCore/DSLThread.h>

//#include "DSLThread.h"
#include "CNSLPlugin.h"

class CNSLNodeLookupThread : public DSLThread
{
public:
                CNSLNodeLookupThread			( CNSLPlugin* parentPlugin );
    virtual		~CNSLNodeLookupThread			();
    
	virtual void*			Run					( void ) = 0;

            void			AddResult			( CFStringRef newNodeName );
            void			AddResult			( const char* newNodeName );
    
            CNSLPlugin*		GetParentPlugin		( void ) { return mParentPlugin; }

protected:

private:
        CNSLPlugin*			mParentPlugin;
};
#endif		// #ifndef