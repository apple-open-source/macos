/*
 *  CSLPServiceLookupThread.h
 *
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CSLPServiceLookupThread_
#define _CSLPServiceLookupThread_ 1

//#include <Carbon/Carbon.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "SLPDefines.h"

#include "CNSLServiceLookupThread.h"

class CSLPServiceLookupThread : public CNSLServiceLookupThread
{
public:
                            CSLPServiceLookupThread		( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual					~CSLPServiceLookupThread	();
        
	virtual void*			Run							( void );

protected:
#ifdef TURN_ON_RECENTS_FAVORITES
            void				GetRecFavServices		( Boolean recents, char* serviceType, CNSLDirNodeRep* nodeToSearch );
#endif

private:
            SLPHandle		mSLPRef;
};
#endif		// #ifndef