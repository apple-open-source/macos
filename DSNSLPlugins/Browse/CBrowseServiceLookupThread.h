/*
 *  CBrowseServiceLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CBrowseServiceLookupThread_
#define _CBrowseServiceLookupThread_ 1

#include "zonelist.h"
#include "serverlist.h"

#include "CNSLServiceLookupThread.h"

class CBrowseServiceLookupThread : public CNSLServiceLookupThread
{
public:
                            CBrowseServiceLookupThread				( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual					~CBrowseServiceLookupThread			();
        
	virtual void*			Run									( void );
    
            void			SetDefaultNeighborhoodNamePtr		( const char *name );

protected:
            OSStatus		DoLookupOnService					( char* service, char *zone );

private:
            CFStringRef				mServiceListRef;
            char*					mBuffer;
    const	char*					mDefaultNeighborhoodName;
};
#endif		// #ifndef